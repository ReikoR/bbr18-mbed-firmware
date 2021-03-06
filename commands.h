#ifndef BBR18_MBED_FIRMWARE_COMMANDS_H
#define BBR18_MBED_FIRMWARE_COMMANDS_H

struct __attribute__((packed)) RobotCommand {
    int16_t speed1;
    int16_t speed2;
    int16_t speed3;
    int16_t speed4;
    int16_t speed5;
    char fieldID;
    char robotID;
    uint8_t shouldSendAck;
    uint8_t led;
};

struct __attribute__((packed)) Feedback {
    int16_t speed1;
    int16_t speed2;
    int16_t speed3;
    int16_t speed4;
    int16_t speed5;
    uint8_t ball1;
    uint8_t ball2;
    uint16_t distance;
    uint8_t isSpeedChanged;
    char refereeCommand;
    uint8_t button;
    int time;
};

#endif //BBR18_MBED_FIRMWARE_COMMANDS_H
