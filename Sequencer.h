#pragma once

#define NUM_CHANNELS 8

void SequencerInputMidiEvent(MidiEvent event);
void SequencerOkEvent();
void SequencerChannelOnOffToggle(uint8_t channel);
bool SequencerGetChannelOnOff(uint8_t channel);
void SequencerClear(uint8_t channel);
void SequencerRepeaterOnOff(bool on);

void SequencerReset();
void SequencerBackgroundTasks();
void SequencerInit();
