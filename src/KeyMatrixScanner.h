#pragma once

#define MAX_KEY_PORTS_IN     4
#define MAX_KEY_PORTS_OUT    4

#define GROUND_PIN           -1

typedef struct KeyState_
{
    bool state;
    bool stateChangeFlag;
}KeyState;

typedef struct KeyMatrix_
{
    const int inPorts[MAX_KEY_PORTS_IN];
    const int outPorts[MAX_KEY_PORTS_OUT];
    const int numInPorts;
    const int numOutPorts;
    const int numKeys;
    KeyState keyStates[MAX_KEY_PORTS_IN * MAX_KEY_PORTS_OUT];
    int keyMap[MAX_KEY_PORTS_IN][MAX_KEY_PORTS_OUT];
    void (*callback)(uint8_t key, bool state);
}KeyMatrix;

void KeyMatricesInit();
void KeyMatrixCallbacks();
