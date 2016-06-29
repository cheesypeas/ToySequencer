#pragma once

enum MidiEventType
{
    MIDI_NOTE_ON,
    MIDI_NOTE_OFF,
};

typedef struct MidiEvent_
{
    MidiEventType type;
    uint8_t channel;
    uint8_t value;
    uint8_t velocity;
}MidiEvent;

MidiEvent BuildMidiEvent(MidiEventType midiEventType, uint8_t channel, uint8_t value, uint8_t velocity);

void CommsInit();
void PrintFormat(char *fmt, ... );
void OutputMidiEvent(MidiEvent event);
