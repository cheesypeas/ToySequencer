#include "Arduino.h"
#include "DueTimer.h"
#include "Comms.h"
#include "KeyMatrices.h"

volatile bool userInputflag;


static void ScanKeys()
{
    for (int matrix = 0; matrix < NUM_KEY_MATRICES; matrix++)
    {
        for (int in = 0; in < keyMatrices[matrix].numInPorts; in++)
        {
            for (int out = 0; out < keyMatrices[matrix].numOutPorts; out++)
            {
                if(keyMatrices[matrix].outPorts[out] != GROUND_PIN)
                {
                    digitalWrite(keyMatrices[matrix].outPorts[out], LOW);
                }
                int key = keyMatrices[matrix].keyMap[in][out];
                bool prevState = keyMatrices[matrix].keyStates[key].state;
                bool curState = !digitalRead(keyMatrices[matrix].inPorts[in]);

                if (curState != prevState)
                {
                    keyMatrices[matrix].keyStates[key].stateChangeFlag = true;
                    keyMatrices[matrix].keyStates[key].state = curState;
                }

                digitalWrite(keyMatrices[matrix].outPorts[out], HIGH);

                pinMode(keyMatrices[matrix].inPorts[in], OUTPUT);
                digitalWrite(keyMatrices[matrix].inPorts[in], HIGH);
                pinMode(keyMatrices[matrix].inPorts[in], INPUT_PULLUP);

            }
        }
    }
}


void KeyMatrixCallbacks()
{    
    for (int matrix = 0; matrix < NUM_KEY_MATRICES; matrix++)
    {
        for (int key = 0; key < keyMatrices[matrix].numKeys; key++)
        {
            if (keyMatrices[matrix].keyStates[key].stateChangeFlag)
            {
                keyMatrices[matrix].callback(key, keyMatrices[matrix].keyStates[key].state);
                keyMatrices[matrix].keyStates[key].stateChangeFlag = false;
            }
        }
    }   
}


void KeyMatricesInit()
{
    for (int matrix = 0; matrix < NUM_KEY_MATRICES; matrix++)
    {
        for (int in = 0; in < keyMatrices[matrix].numInPorts; in++)
        {
            pinMode(keyMatrices[matrix].inPorts[in], INPUT_PULLUP);
        }
        for (int out = 0; out < keyMatrices[matrix].numOutPorts; out++)
        {
            pinMode(keyMatrices[matrix].outPorts[out], OUTPUT);
            digitalWrite(keyMatrices[matrix].outPorts[out], HIGH);
        }
    }
    Timer1.attachInterrupt(ScanKeys).start(5000);
}
