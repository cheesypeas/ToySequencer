#pragma once

#define MAX_SOURCE_PORTS   4
#define MAX_SINK_PORTS     2

typedef struct LedMatrix_
{
    const int sourcePorts[MAX_SOURCE_PORTS];
    const int sinkPorts[MAX_SINK_PORTS];
    const int numSources;
    const int numSinks;
    const int numLeds;
    bool ledStates[MAX_SOURCE_PORTS * MAX_SINK_PORTS];
    int curSource;
    int curSink;
    int ledMap[MAX_SOURCE_PORTS][MAX_SINK_PORTS];
}LedMatrix;


void LedsInit();
void LedOnOff(int matrix, int led, bool state);
void LedOn(int matrix, int led);
void LedOff(int matrix, int led);
