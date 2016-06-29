#include "Arduino.h"

#include "DueTimer.h"

#include "Leds.h"
#include "LedMatrices.h"


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


void LedPwm()
{
    for (int matrix = 0; matrix < NUM_LED_MATRICES; matrix++)
    {
        int sink = ledMatrices[matrix].curSink;
        
        digitalWrite(ledMatrices[matrix].sinkPorts[sink], HIGH);
        sink = (sink + 1) % ledMatrices[matrix].numSinks;
        digitalWrite(ledMatrices[matrix].sinkPorts[sink], LOW);
        ledMatrices[matrix].curSink = sink;
        
        for (int source = 0; source < ledMatrices[matrix].numSources; source++)
        {
            int curLed = ledMatrices[matrix].ledMap[source][sink];
            if (ledMatrices[matrix].ledStates[curLed])
            {
                digitalWrite(ledMatrices[matrix].sourcePorts[source], HIGH);
            }
            else
            {
                digitalWrite(ledMatrices[matrix].sourcePorts[source], LOW);
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
}

