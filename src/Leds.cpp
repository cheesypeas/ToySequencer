#include "Arduino.h"

#include "DueTimer.h"

#include "Leds.h"
#include "LedMatrices.h"

// todo: int to intx_t ? 

void LedOnOff(int matrix, int led, bool state)
{
    ledMatrices[matrix].ledStates[led] = state;
}


void LedOn(int matrix, int led)
{
    LedOnOff(matrix, led, true);
}


void LedOff(int matrix, int led)
{
    LedOnOff(matrix, led, false);
}


void LedFlash(int matrix, int led, int numFlashes)
{
    ledMatrices[matrix].numFlashes[led] = numFlashes;
}


void DigitalWriteIgnoreFixedPins(int pin, bool state)
{
    if (pin < 0)
    {
        return;
    }
    digitalWrite(pin, state);
}


void LedPwm()
{
    for (int matrix = 0; matrix < NUM_LED_MATRICES; matrix++)
    {
        int sink = ledMatrices[matrix].curSink;

        DigitalWriteIgnoreFixedPins(ledMatrices[matrix].sinkPorts[sink], HIGH);
        sink = (sink + 1) % ledMatrices[matrix].numSinks;
        ledMatrices[matrix].curSink = sink;
        bool anyLedsOn = false;

        for (int source = 0; source < ledMatrices[matrix].numSources; source++)
        {
            int curLed = ledMatrices[matrix].ledMap[source][sink];
            if (ledMatrices[matrix].ledStates[curLed])
            {
                anyLedsOn = true;
                DigitalWriteIgnoreFixedPins(ledMatrices[matrix].sourcePorts[source], HIGH);
            }
            else
            {
                DigitalWriteIgnoreFixedPins(ledMatrices[matrix].sourcePorts[source], LOW);
            }
        }

        if (anyLedsOn)
        {
            DigitalWriteIgnoreFixedPins(ledMatrices[matrix].sinkPorts[sink], LOW);
        }
        else
        {
            DigitalWriteIgnoreFixedPins(ledMatrices[matrix].sinkPorts[sink], HIGH);
        }
    }
}


void LedFlasher()
{
    for (int matrix = 0; matrix < NUM_LED_MATRICES; matrix++)
    {
        for (int led = 0; led < ledMatrices[matrix].numLeds; led++)
        {
            if (ledMatrices[matrix].numFlashes[led] > 0)
            {
                ledMatrices[matrix].ledStates[led] = !ledMatrices[matrix].ledStates[led];
                if (!ledMatrices[matrix].ledStates[led])
                {
                    ledMatrices[matrix].numFlashes[led]--;
                }
            }
        }
    }
}


void LedsInit()
{
    for (int matrix = 0; matrix < NUM_LED_MATRICES; matrix++)
    {
        for (int source = 0; source < ledMatrices[matrix].numSources; source++)
        {
            pinMode(ledMatrices[matrix].sourcePorts[source], OUTPUT);
            digitalWrite(ledMatrices[matrix].sourcePorts[source], HIGH);
        }
        for (int sink = 0; sink < ledMatrices[matrix].numSinks; sink++)
        {
            pinMode(ledMatrices[matrix].sinkPorts[sink], OUTPUT);
            digitalWrite(ledMatrices[matrix].sinkPorts[sink], HIGH);
        }
    }
    Timer3.attachInterrupt(LedPwm).start(5000);
    Timer4.attachInterrupt(LedFlasher).start(61000);
}

