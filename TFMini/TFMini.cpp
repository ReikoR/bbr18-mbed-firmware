#include <mbed-os/targets/TARGET_NXP/TARGET_LPC176X/device/cmsis.h>
#include "TFMini.h"

TFMini::TFMini(PinName txPinName, PinName rxPinName):
        serial(txPinName, rxPinName) {

    messageAvailable = false;
    receiveCounter = 0;
    commandLength = 9;

    if (rxPinName == P2_1) {
        serialId = 1;
    } else if (rxPinName == P0_11) {
        serialId = 2;
    } else {
        serialId = 0;
    }

    serial.attach(this, &TFMini::rxHandler);
}

void TFMini::baud(int baudrate) {
    serial.baud(baudrate);
}

void TFMini::rxHandler(void) {
    // Interrupt does not work with RTOS when using standard functions (getc, putc)
    // https://developer.mbed.org/forum/bugs-suggestions/topic/4217/

    while (serial.readable()) {
        char c = serialReadChar();

        if (receiveCounter < commandLength) {
            if (receiveCounter == 0 || receiveCounter == 1) {
                // Do not continue before 0x59 is received
                if (c == 0x59) {
                    receiveBuffer[receiveCounter] = c;
                    receiveCounter++;
                } else {
                    receiveCounter = 0;
                }
            } else {
                receiveBuffer[receiveCounter] = c;
                receiveCounter++;
            }

            if (receiveCounter == commandLength) {
                receiveCounter = 0;

                for (unsigned int i = 0; i < commandLength; i++) {
                    receivedMessage[i] = receiveBuffer[i];
                }

                messageAvailable = true;
            }
        }
    }
}

bool TFMini::readable() {
    return messageAvailable;
}

char *TFMini::read() {
    messageAvailable = false;
    return receivedMessage;
}

char TFMini::serialReadChar() {
    if (serialId == 1) {
        return LPC_UART1->RBR;
    }

    if (serialId == 2) {
        return LPC_UART2->RBR;
    }

    return LPC_UART0->RBR;
}