#include "mbed.h"
#include "EthernetInterface.h"
#include "LedManager.h"

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

LedManager leds(P0_9);

Ticker heartbeatTicker;

char recvBuffer[64];
char ethSendBuffer[64];

float heartBeatPeriod = 0.5;
bool isHeartbeatUpdate = false;

void heartbeatTick() {
    isHeartbeatUpdate = true;
}

void onUDPSocketData(void* buffer, int size) {

}

int main() {
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
        if (isHeartbeatUpdate) {
            isHeartbeatUpdate = false;

            if (blinkState) {
                leds.setLedColor(0, LedManager::BLUE);
            } else {
                leds.setLedColor(0, LedManager::YELLOW);
            }

            leds.update();

            blinkState = !blinkState;

            socket.sendto(PC_IP_ADDRESS, PORT, "Test", 4);

            //int charCount = sprintf(ethSendBuffer, "Test");
            //socket.sendto(PC_IP_ADDRESS, PORT, ethSendBuffer, charCount);
        }

        nsapi_size_or_error_t size = socket.recvfrom(&address, recvBuffer, sizeof recvBuffer);

        if (size < 0 && size != NSAPI_ERROR_WOULD_BLOCK) {
            //pc.printf("recvfrom failed with error code %d\n", size);
        } else if (size > 0) {
            recvBuffer[size] = '\0';
            //pc.printf("recv %d [%s] from %s:%d\n", size, recvBuffer, address.get_ip_address(), address.get_port());

            onUDPSocketData(recvBuffer, size);
        }
    }
}