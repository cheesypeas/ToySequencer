#pragma once

#define NUM_CHANNELS 8

void SequencerInputMidiEvent(MidiEvent event);
void SequencerLoopEvent();
void SequencerChannelOnOff(uint8_t channel);
bool SequencerGetChannelOnOff(uint8_t channel);
void SequencerClearChannel(uint8_t channel);

void SequencerReset();
void SequencerBackgroundTasks();
void SequencerInit();
