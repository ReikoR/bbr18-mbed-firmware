const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const mbedPort = 8042;
const mbedAddress = '192.168.4.1';

var currentTime = Date.now();
var prevTime = currentTime;

//var speeds = [0, 0, 0, 0, 0];
var maxSpeeds = [1000, 1000, 1000, 1000, 16000];
var minSpeeds = [-1000, -1000, -100, -1000, -16000];
var speedsSteps = [10, 10, 10, 10, 50];
var speedDirectionsUp = [true, true, true, true, true];
var feedback = {};
const commandBuffer = Buffer.alloc(14);
const commandObject =  {
    speeds: [0, 0, 0, 0, 0],
    fieldID: 'A',
    robotID: 'B',
    shouldSendAck: false,
    led: 2
};

let lastButton = 0;

/*
int16_t speed1;
int16_t speed2;
int16_t speed3;
int16_t speed4;
int16_t speed5;
char fieldID;
char robotID;
uint8_t shouldSendAck;
uint8_t led;
 */

function updateCommandBuffer() {
    const speeds = commandObject.speeds;

    for (let i = 0; i < speeds.length; i++) {
        commandBuffer.writeInt16LE(speeds[i], 2 * i);
    }

    commandBuffer.writeUInt8(commandObject.fieldID.charCodeAt(0), 10);
    commandBuffer.writeUInt8(commandObject.robotID.charCodeAt(0), 11);
    commandBuffer.writeUInt8(commandObject.shouldSendAck ? 1 : 0, 12);
    commandBuffer.writeUInt8(commandObject.led, 13);

    commandObject.shouldSendAck = false;
}

function clone(obj) {
    var cloned = {};

    for (let key in obj) {
        cloned[key] = obj[key];
    }

    return cloned;
}

socket.on('error', (err) => {
    console.log(`socket error:\n${err.stack}`);
    socket.close();
});

socket.on('message', (msg, rinfo) => {
    //console.log(`socket got: ${msg} from ${rinfo.address}:${rinfo.port}`);

    currentTime = Date.now();

    /*int16_t speed1;
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
    int time;*/

    feedback = {
        speed1: msg.readInt16LE(0),
        speed2: msg.readInt16LE(2),
        speed3: msg.readInt16LE(4),
        speed4: msg.readInt16LE(6),
        speed5: msg.readInt16LE(8),
        ball1: msg.readUInt8(10),
        ball2: msg.readUInt8(11),
        distance: msg.readUInt16LE(12),
        isSpeedChanged: msg.readUInt8(14),
        refereeCommand: String.fromCharCode(msg.readUInt8(15)),
        button: msg.readUInt8(16),
        time: msg.readInt32LE(17),
    };

    if (feedback.refereeCommand === 'P') {
        commandObject.shouldSendAck = true;
    } else if (feedback.refereeCommand === 'S') {

    } else if (feedback.refereeCommand === 'T') {

    }

    if (lastButton === 0 && feedback.button === 1) {
        switch (commandObject.led) {
            case 0:
                commandObject.led = 1;
                break;
            case 1:
                commandObject.led = 0;
                break;
            default:
                commandObject.led = 1;
        }
    }

    lastButton = feedback.button;

    //console.log(('0' + (currentTime - prevTime)).slice(-2), feedback);
    console.log(('000' + (currentTime - prevTime)).slice(-4), feedback.time, feedback.refereeCommand);

    prevTime = currentTime;
});

socket.on('listening', () => {
    const address = socket.address();
    console.log(`socket listening ${address.address}:${address.port}`);

    var value = 0;
    const speeds = commandObject.speeds;

    setInterval(function () {
        for (let i = 0; i < speeds.length; i++) {
            let newSpeed = speeds[i] + (speedDirectionsUp[i] ? speedsSteps[i] : -speedsSteps[i]);

            if (speedDirectionsUp[i] && newSpeed > maxSpeeds[i]) {
                newSpeed = maxSpeeds[i];
                speedDirectionsUp[i] = false;
            } else if (!speedDirectionsUp[i] && newSpeed < minSpeeds[i]) {
                newSpeed = minSpeeds[i];
                speedDirectionsUp[i] = true;
            }

            speeds[i] = newSpeed;
        }

        speeds[0] = 0;
        speeds[1] = 0;
        speeds[2] = 0;
        speeds[3] = 0;
        speeds[4] = 0;

        updateCommandBuffer();
        //console.log(commandObject);
        //console.log(commandBuffer);
        socket.send(commandBuffer, 0, commandBuffer.length, mbedPort, mbedAddress);

        //pipeMotorSpeed = Math.sin(value) * 1000;
        //value += 0.005;
    }, 500);
});

socket.bind(8042);