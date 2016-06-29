#include <stdint.h>

#include "Comms.h"
#include "Controller.h"
#include "Leds.h"
#include "Sequencer.h"

#define JOYSTICK_L                   2
#define JOYSTICK_R                   1
#define JOYSTICK_D                   0
#define JOYSTICK_U                   3

#define NUM_KEYS                     8
#define NUM_NOTE_SETS                16
#define NUM_CHANNELS                 8

#define DEFAULT_VELOCITY             100

enum ControllerMode
{
    PLAY_NOTES = 0,
    PER_CHANNEL_ON_OFF,
    // New modes go here..
    
    NUM_PIANO_KEY_MODES,
};

// A set of notes (NoteSet) is a group of notes assignable to keys 0 -> NUM_KEYS -1
uint8_t noteSets[NUM_NOTE_SETS][NUM_KEYS] = {};

uint8_t controllerMode;
int8_t channelInFocus = 0;  // per note
int8_t setOfNotesInFocus[NUM_CHANNELS] = {}; //per note


static void RefreshLedOutput()
{
    for (int key = 0; key < NUM_KEYS; key++)
    {
        LedOff(0, key);
    }

    uint8_t mode = controllerMode;
    if (mode == PER_CHANNEL_ON_OFF)
    {
        for (int key = 0; key < NUM_KEYS; key++)
        {
            if (SequencerGetChannelOnOff(key))
            {
                LedOn(0, key);
            }
        }
    }
}


void LedOutputEvent(MidiEvent event)
{
    if (controllerMode != PLAY_NOTES)
    {
        return;
    }
    
    if (event.channel != channelInFocus)
    {
        return;
    }
    
    int noteSet = setOfNotesInFocus[event.channel];

    for (int key = 0; key < NUM_KEYS; key++)
    {
        if (noteSets[noteSet][key] == event.value)
        {
            switch (event.type)
            {
                case MIDI_NOTE_ON:
                    LedOn(0, key);
                    break;

                case MIDI_NOTE_OFF:
                    LedOff(0, key);
                    break;

                default:
                    break;
            }
        }
    }
}


void ControllerOutputEvent(MidiEvent event)
{
    OutputMidiEvent(event);
    LedOutputEvent(event);
}


void ControllerOutputChannelOnOff(uint8_t channel, bool on)
{
    if (controllerMode != PER_CHANNEL_ON_OFF)
    {
        return;
    }
        
    LedOnOff(0, channel, on);
}


void PianoKeyInput(uint8_t key, bool state)
{
    uint8_t localControllerMode = controllerMode;
    uint8_t * localNoteSet = noteSets[setOfNotesInFocus[channelInFocus]];
    switch (localControllerMode)
    {
        case PLAY_NOTES:
        {
            MidiEventType eventType = state ? MIDI_NOTE_ON : MIDI_NOTE_OFF;
            MidiEvent event = BuildMidiEvent(eventType, channelInFocus, localNoteSet[key] , DEFAULT_VELOCITY);
            ControllerOutputEvent(event);
            SequencerInputEvent(event);
            break;
        }
        
        case PER_CHANNEL_ON_OFF:
        {
            if (state)
            {
                SequencerChannelOnOff(key);
            }
            break;
        }
        
        default:
            break;
    }
}


// Shift the note set of the channel in focus up or down
static void SetOfNotesShift(int8_t shift)
{
    int8_t channel = channelInFocus;
    int8_t setOfNotes = setOfNotesInFocus[channel];

    //TODO: if piano key pressed do something sensible like eg return and ignore press
    
    setOfNotes += shift;
    if (setOfNotes >= NUM_NOTE_SETS)
    {
        setOfNotes = NUM_NOTE_SETS - 1;
    }
    else if (setOfNotes < 0)
    {
        setOfNotes = 0;
    }
       
    setOfNotesInFocus[channel] = setOfNotes;
    RefreshLedOutput();
    #ifdef DEBUG
    PrintFormat("Note Set shift: setOfNotes: %d, channel: %d\n", setOfNotesInFocus[channel], channelInFocus);
    #endif
}


static void ChannelShift(int8_t shift)
{
    int8_t channel = channelInFocus;

    //TODO: if piano key pressed do something sensible like eg return and ignore press
   
    channel += shift;
    if (channel >= NUM_CHANNELS)
    {
        channel = NUM_CHANNELS - 1;
    }
    else if (channel < 0)
    {
        channel = 0;
    }

    channelInFocus = channel;
    RefreshLedOutput();
    #ifdef DEBUG
    PrintFormat("Channel shift: setOfNotes: %d, channel: %d\n", setOfNotesInFocus[channel], channelInFocus);
    #endif
}


void JoystickInput(uint8_t key, bool state)
{
    if (state)
    {
        switch (key)
        {
            case JOYSTICK_L:
                SetOfNotesShift(-1);
                break;
            case JOYSTICK_R:
                SetOfNotesShift(1);
                break;
            case JOYSTICK_D:
                ChannelShift(-1);
                break;
            case JOYSTICK_U:
                ChannelShift(1);
                break; 
        }
        PrintFormat("Joystick on: %d\n", key);
    }
    else
    {
        PrintFormat("Joystick off\n");
    }
}


static void CycleControllerMode()
{
    uint8_t localControllerMode = controllerMode;
    localControllerMode++;
    if (localControllerMode >= NUM_PIANO_KEY_MODES)
    {
        localControllerMode = 0;
    }
    controllerMode = localControllerMode;

    RefreshLedOutput();
    PrintFormat("Cycle controller mode: %d\n", localControllerMode);
}


void MainButtonsInput(uint8_t key, bool state)
{
    #ifdef DEBUg
    PrintFormat("Main button %d %s\n", key, state ? "on" : "off");
    #endif
    switch (key)
    {
        case 0:
            if (state)
            {
                SequencerTapTempo();
            }
            break;
        case 1:
            if (state)
            {
                SequencerStartStop();
            }
            break;
        case 2:
            if (state)
            {
                CycleControllerMode();
            }
            break;
        case 3:
            if (state)
            {
                SequencerStoreLastCycle();
            }
            break;            
    }
}


void OtherButtonsInput(uint8_t key, bool state)
{
    #ifdef DEBUG
    PrintFormat("Other button input: %d\n", key);
    #endif
    if (state)
    {
        switch (key)
        {
            case 0: // Big button
                break;
            case 1: // Turntable
                SequencerClearChannel(channelInFocus);
                break;
            case 2: // Roller ball
                break;
            default:
                break;
        }
    }
}


// Initialise the note sets with sequential values starting at 0.
void InitNoteSetsSequential()
{
    int value = 0;
    
    for (int set = 0; set < NUM_NOTE_SETS; set++)
    {
        for (int key = 0; key < NUM_KEYS; key++)
        {
            noteSets[set][key] = value++;
        }
    }
}


void ControllerInit()
{
    InitNoteSetsSequential();
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        setOfNotesInFocus[i] = 7;
    }
}


