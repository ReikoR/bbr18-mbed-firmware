#ifndef MBED_TEST_TFMINI_H
#define MBED_TEST_TFMINI_H

#include "mbed.h"

class TFMini {
protected:
    FunctionPointer _callback;

public:
    TFMini(PinName txPinName, PinName rxPinName);

    void baud(int baudrate);

    char *read();

    bool readable();

    void attach(void (*function)(void)) {
        _callback.attach(function);
    }

    template<typename T>
    void attach(T *object, void (T::*member)(void)) {
        _callback.attach( object, member );
    }

private:
    Serial serial;

    int serialId;

    void rxHandler(void);

    bool messageAvailable;

    char serialReadChar();

    unsigned int receiveCounter;
    char receiveBuffer[16];

    char receivedMessage[16];

    int commandLength;
};

#endif //MBED_TEST_TFMINI_H
