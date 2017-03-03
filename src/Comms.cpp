#include <stdarg.h>

#include "Arduino.h"
#include "Comms.h"

//#define DEBUG


void CommsInit()
{
    #ifdef DEBUG
    Serial.begin(112500);
    #else
    Serial.begin(31250);
    #endif
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
    
    Serial.print(buf);
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
            #ifdef DEBUG
            PrintFormat("Channel %d, Note ON: ", event.channel);
            #else
            Serial.write(0x90 | event.channel);
            #endif
            break;
        case MIDI_NOTE_OFF:
            #ifdef DEBUG
            PrintFormat("Channel %d, Note OFF: ", event.channel);
            #else
            Serial.write(0x80 | event.channel);
            #endif
            break;
        default:
            #ifdef DEBUG
            PrintFormat("Unrecognised midi event, %d: ", event.type);
            #endif
            return;
    }

    #ifdef DEBUG
    PrintFormat("%d, vel: %d\n", event.value, event.velocity);
    #else
    Serial.write(event.value);
    Serial.write(event.velocity);
    #endif
}


