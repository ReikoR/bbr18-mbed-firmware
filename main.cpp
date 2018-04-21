#include "mbed.h"
#include "EthernetInterface.h"
#include "commands.h"
#include "LedManager.h"
#include "MotorDriverManagerRS485.h"
#include "TFMini.h"

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

MotorDriverManagerRS485 motors(P2_0, P2_1);
TFMini tfMini(P0_10, P0_11);
LedManager leds(P0_9);

DigitalIn ball1(P1_29);
DigitalIn ball2(P2_4);

Ticker heartbeatTicker;

char recvBuffer[64];
char ethSendBuffer[64];

float heartBeatPeriod = 0.5;
bool isHeartbeatUpdate = false;

bool returnSpeeds = true;

int ball1State = 0;
int ball2State = 0;

uint8_t isSpeedChanged = 0;
void heartbeatTick() {
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
    isSpeedChanged = 0;

    socket.sendto(PC_IP_ADDRESS, PORT, &feedback, sizeof feedback);
}

void onUDPSocketData(void* buffer, int size) {
    if (sizeof(SpeedCommand) == size) {
        SpeedCommand *command = static_cast<SpeedCommand *>(buffer);

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

    tfMini.baud(115200);
    leds.setLedColor(0, LedManager::OFF);
    leds.setLedColor(1, LedManager::OFF);
    leds.update();

    eth.set_network(MBED_IP_ADDRESS, "255.255.255.0", PC_IP_ADDRESS);
    eth.connect();

    socket.set_blocking(false);
    socket.open(&eth);
    socket.bind(PORT);

    SocketAddress address;

    heartbeatTicker.attach(&heartbeatTick, heartBeatPeriod);

    bool blinkState = false;

    leds.setLedColor(0, LedManager::GREEN);
    leds.setLedColor(1, LedManager::GREEN);
    leds.update();

    while (true) {
        motors.update();

        if (isHeartbeatUpdate) {
            isHeartbeatUpdate = false;

            if (blinkState) {
                leds.setLedColor(0, LedManager::BLUE);
            } else {
                leds.setLedColor(0, LedManager::YELLOW);
            }

            leds.update();

            blinkState = !blinkState;
        }

        nsapi_size_or_error_t size = socket.recvfrom(&address, recvBuffer, sizeof recvBuffer);

        if (size < 0 && size != NSAPI_ERROR_WOULD_BLOCK) {
            //pc.printf("recvfrom failed with error code %d\n", size);
        } else if (size > 0) {
            recvBuffer[size] = '\0';
            //pc.printf("recv %d [%s] from %s:%d\n", size, recvBuffer, address.get_ip_address(), address.get_port());

            onUDPSocketData(recvBuffer, size);
        }

        int newBall1State = ball1;
        bool isBallStateChanged = false;

        if (ball1State != newBall1State) {
            ball1State = newBall1State;
            isBallStateChanged = true;
        }

        int newBall2State = ball2;

        if (ball2State != newBall2State) {
            ball2State = newBall2State;
            isBallStateChanged = true;
        }

        if (isBallStateChanged) {
            sendFeedback();
        }
    }
}