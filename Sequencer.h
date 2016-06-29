#pragma once

#define NUM_CHANNELS 16

void SequencerInputEvent(MidiEvent event);
void SequencerDeactivateNote(uint8_t key);
void SequencerChannelOnOff(uint8_t channel);
bool SequencerGetChannelOnOff(uint8_t channel);
void SequencerTapTempo();
void SequencerStartStop();
void SequencerStoreLastCycle();
void SequencerClearChannel(uint8_t channel);

void SequencerInit();
void SequencerBackgroundTasks();
