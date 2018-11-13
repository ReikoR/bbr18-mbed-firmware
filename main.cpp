#include "mbed.h"
#include "EthernetInterface.h"
#include "commands.h"
#include "MotorDriverManagerRS485.h"
#include "TFMini.h"
#include "RFManager.h"

#define PORT 8042
#define MBED_IP_ADDRESS "192.168.4.1"
#define PC_IP_ADDRESS "192.168.4.8"

EthernetInterface eth;
UDPSocket socket;

extern "C" void mbed_mac_address(char *s) {
    char mac[6];
    mac[0] = 0x00;
    mac[1] = 0x02;
    mac[2] = 0xf7;
    mac[3] = 0xf0;
    mac[4] = 0x45;
    mac[5] = 0xbe;
    // Write your own mac address here
    memcpy(s, mac, 6);
}

char fieldID = 'Z';
char robotID = 'Z';

MotorDriverManagerRS485 motors(P2_0, P2_1);
TFMini tfMini(P0_0, P0_1);
RFManager xbee(P0_10, P0_11);

DigitalIn ball1(P2_12);
DigitalIn ball2(P2_13);

DigitalIn button(P1_29);

Ticker heartbeatTicker;

DigitalOut ledBasket(P0_18);
DigitalOut ledButton(P0_19);
DigitalOut ledHeartbeat(P0_20);

char recvBuffer[64];
char ethSendBuffer[64];

us_timestamp_t heartBeatPeriod_us = 1000;
bool isHeartbeatUpdate = false;
bool updateLeds = false;

bool returnSpeeds = true;

bool failSafeEnabled = true;
int failSafeCountMotors = 0;
int failSafeLimitMotors = 500;

int ledCount = 500;
int ledCounter = 0;

enum ButtonState {
    buttonOff, buttonDown, buttonActive
};

enum ButtonResult {
    buttonIdle = 0,
    buttonPressed = 1,
    buttonPressedLong = 2
};

int buttonState = buttonOff;
int currentButton = button;
int buttonDebounceCount = 50;
int buttonDebounceCounter = buttonDebounceCount + 1;
int buttonActiveCount = 2000;
int buttonActiveCounter = 0;
ButtonResult buttonResult = buttonIdle;

int ball1State = 0;
int ball2State = 0;

enum LedBasketState {
    basketMagenta = 0,
    basketBlue = 1,
    basketUnknown = 2
};

LedBasketState currentLedBasketState = basketUnknown;

uint8_t isSpeedChanged = 0;
Timer runningTime;

char lastParsedRefereeCommand[3];
bool shouldSendAck = false;
bool hasRefereeCommandChanged = false;
char refereeAckCommand[] = "aZZACK------";

void heartbeatTick() {
    isHeartbeatUpdate = true;

    if (ledCounter++ > ledCount) {
        ledCounter = 0;

        updateLeds = true;
    }

    isHeartbeatUpdate = true;
}

void sendFeedback() {
    int* speeds = motors.getSpeeds();

    Feedback feedback{};
    feedback.speed1 = static_cast<int16_t>(speeds[0]);
    feedback.speed2 = static_cast<int16_t>(speeds[1]);
    feedback.speed3 = static_cast<int16_t>(speeds[2]);
    feedback.speed4 = static_cast<int16_t>(speeds[3]);
    feedback.speed5 = static_cast<int16_t>(speeds[4]);
    feedback.ball1 = static_cast<uint8_t>(ball1);
    feedback.ball2 = static_cast<uint8_t>(ball2);
    feedback.distance = tfMini.read()->distance;
    feedback.isSpeedChanged = isSpeedChanged;
    feedback.refereeCommand = lastParsedRefereeCommand[2];
    feedback.button = buttonResult;
    feedback.time = runningTime.read_us();

    isSpeedChanged = 0;

    socket.sendto(PC_IP_ADDRESS, PORT, &feedback, sizeof feedback);
}

bool areCharsEqual(char* buf1, char* buf2, int length) {
    for (int i = 0; i < length; i++) {
        if (buf1[i] != buf2[i]) {
            return false;
        }
    }

    return true;
}

void handleRefereeCommand(char* refereeCommand) {
    char* start = const_cast<char *>("START");
    char* stop = const_cast<char *>("STOP");
    char* ping = const_cast<char *>("PING");

    if (refereeCommand[1] == fieldID && refereeCommand[2] == robotID) {
        lastParsedRefereeCommand[0] = refereeCommand[1]; // Robot ID
        lastParsedRefereeCommand[1] = refereeCommand[2]; // Field ID

        char prevRefereeCommand = lastParsedRefereeCommand[2];

        if (areCharsEqual(refereeCommand + 3, start, 5)) {
            lastParsedRefereeCommand[2] = 'S';
        } else if (areCharsEqual(refereeCommand + 3, stop, 4)) {
            lastParsedRefereeCommand[2] = 'T';
        } else if (areCharsEqual(refereeCommand + 3, ping, 4)) {
            lastParsedRefereeCommand[2] = 'P';
        }

        if (prevRefereeCommand != lastParsedRefereeCommand[2]) {
            hasRefereeCommandChanged = true;
        }
    }
}

void onUDPSocketData(void* buffer, int size) {
    if (sizeof(RobotCommand) == size) {
        failSafeCountMotors = 0;
        returnSpeeds = true;

        RobotCommand *command = static_cast<RobotCommand *>(buffer);

        fieldID = command->fieldID;
        robotID = command->robotID;

        shouldSendAck = command->shouldSendAck == 1;

        switch (command->led) {
            case 0:
                currentLedBasketState = basketMagenta;
                break;
            case 1:
                currentLedBasketState = basketBlue;
                break;
            case 2:
                currentLedBasketState = basketUnknown;
                break;
        }

        motors.setSpeeds(command->speed1, command->speed2, command->speed3, command->speed4, command->speed5);
    }
}

void handleSpeedsSent() {
    if (returnSpeeds) {
        isSpeedChanged = 1;
        sendFeedback();
    }
}

int main() {
    motors.baud(150000);
    motors.attach(&handleSpeedsSent);

    lastParsedRefereeCommand[2] = 'X';

    xbee.baud(9600);

    tfMini.baud(115200);
    ledBasket = 0;
    ledButton = 0;
    ledHeartbeat = 1;

    eth.set_network(MBED_IP_ADDRESS, "255.255.255.0", PC_IP_ADDRESS);
    eth.connect();

    socket.set_blocking(false);
    socket.open(&eth);
    socket.bind(PORT);

    SocketAddress address;

    heartbeatTicker.attach_us(&heartbeatTick, heartBeatPeriod_us);

    bool blinkState = false;
    bool shouldSendFeedback = true;

    ledHeartbeat = 0;

    runningTime.start();

    while (true) {
        motors.update();

        if (isHeartbeatUpdate) {
            failSafeCountMotors++;
            isHeartbeatUpdate = false;

            if (failSafeCountMotors == failSafeLimitMotors) {
                failSafeCountMotors = 0;

                if (failSafeEnabled) {
                    returnSpeeds = false;
                    motors.setSpeeds(0, 0, 0, 0, 0);
                }
            }

            if (buttonDebounceCounter < buttonDebounceCount) {
                buttonDebounceCounter++;
            } else if (buttonDebounceCounter == buttonDebounceCount) {
                buttonDebounceCounter++;

                if (currentButton) {
                    if (buttonState == buttonDown) {
                        buttonResult = buttonPressed;
                        shouldSendFeedback = true;
                    } else if (buttonState == buttonActive) {
                        buttonResult = buttonPressedLong;
                        shouldSendFeedback = true;
                    }

                    buttonState = buttonOff;
                    ledButton = 0;

                } else  {
                    if (buttonState == buttonOff) {
                        buttonState = buttonDown;
                    }
                }
            }

            if (currentLedBasketState == basketMagenta) {
                ledBasket = 0;
            } else if (currentLedBasketState == basketBlue) {
                ledBasket = 1;
            }

            if (buttonState == buttonDown) {
                buttonActiveCounter++;

                ledButton = 1;

                if (buttonActiveCounter >= buttonActiveCount) {
                    buttonState = buttonActive;
                }
            } else if (buttonState != buttonActive) {
                ledButton = 0;
            }

            if (buttonState != buttonDown) {
                buttonActiveCounter = 0;
            }

            if (updateLeds) {
                updateLeds = false;

                if (blinkState) {
                    ledHeartbeat = 1;

                    ledButton = 0;

                    shouldSendFeedback = true;
                } else {
                    ledHeartbeat = 0;

                    if (buttonState == buttonActive) {
                        ledButton = 1;
                    }
                }

                blinkState = !blinkState;

                if (currentLedBasketState == basketUnknown) {
                    if (blinkState) {
                        ledBasket = 0;
                    } else {
                        ledBasket = 1;
                    }
                }
            }

        }

        if (xbee.readable()) {
            handleRefereeCommand(xbee.read());
            /*char* refereeCommand = xbee.read();
            lastParsedRefereeCommand[0] = refereeCommand[1];
            lastParsedRefereeCommand[1] = refereeCommand[2];
            lastParsedRefereeCommand[2] = refereeCommand[5];*/
        }

        xbee.update();

        nsapi_size_or_error_t size = socket.recvfrom(&address, recvBuffer, sizeof recvBuffer);

        if (size > 0) {
            recvBuffer[size] = '\0';
            //pc.printf("recv %d [%s] from %s:%d\n", size, recvBuffer, address.get_ip_address(), address.get_port());

            onUDPSocketData(recvBuffer, size);
        }

        int newBall1State = ball1;

        if (ball1State != newBall1State) {
            ball1State = newBall1State;
            shouldSendFeedback = true;
        }

        int newBall2State = ball2;

        if (ball2State != newBall2State) {
            ball2State = newBall2State;
            shouldSendFeedback = true;
        }

        if (shouldSendAck) {
            shouldSendAck = false;
            refereeAckCommand[1] = fieldID;
            refereeAckCommand[2] = robotID;
            xbee.send(refereeAckCommand);
        }

        int newButton = button;

        if (currentButton != newButton) {
            buttonDebounceCounter = 0;
            currentButton = newButton;
        }

        if (shouldSendFeedback || hasRefereeCommandChanged) {
            shouldSendFeedback = false;

            sendFeedback();

            if (hasRefereeCommandChanged) {
                hasRefereeCommandChanged = false;
                lastParsedRefereeCommand[2] = 'X';
            }

            buttonResult = buttonIdle;
        }
    }
}