#include <stdarg.h>

#include "Arduino.h"
#include "Comms.h"

#define DEBUG


void CommsInit()
{
    #ifdef DEBUG
    Serial1.begin(115200);
    #endif
    Serial.begin(31250);
}


void PrintFormat(const char * fmt, ... )
{
    #ifdef DEBUG
    char buf[128]; // resulting string limited to 128 chars
    va_list args;
    va_start (args, fmt );
    //printf(fmt, args);
    vsnprintf(buf, 128, fmt, args);
    va_end (args);

    Serial1.print(buf);
    #endif
}


MidiEvent BuildMidiEvent(MidiEventType midiEventType, uint8_t channel, uint8_t value, uint8_t velocity)
{
    MidiEvent event = {midiEventType, channel, value, velocity};
    return event;
}


void OutputMidiEvent(MidiEvent event)
{
    switch (event.type)
    {
        case MIDI_NOTE_ON:
            PrintFormat("Channel %d, Note ON: ", event.channel);
            Serial.write(0x90 | event.channel);
            break;
        case MIDI_NOTE_OFF:
            PrintFormat("Channel %d, Note OFF: ", event.channel);
            Serial.write(0x80 | event.channel);
            break;
        default:
            PrintFormat("Unrecognised midi event, %d: ", event.type);
            return;
    }

    PrintFormat("%d, vel: %d\n", event.value, event.velocity);
    Serial.write(event.value);
    Serial.write(event.velocity);
}


