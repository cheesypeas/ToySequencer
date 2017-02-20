#pragma once

#define NUM_CHANNELS 8

void SequencerInputMidiEvent(MidiEvent event);
void SequencerOkEvent();
void SequencerClearEvent(uint8_t channel);
void SequencerResetEvent();

void SequencerChannelOnOffToggle(uint8_t channel);
bool SequencerGetChannelOnOff(uint8_t channel);
void SequencerRepeaterOnOff(bool on);

void SequencerBackgroundTasks();
void SequencerInit();
