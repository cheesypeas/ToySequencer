#include <stdint.h>

#include "DueTimer.h"

#include "Comms.h"
#include "Controller.h"
#include "Leds.h"
#include "Sequencer.h"

//#define DEBUG

#define MAX_NOTES_ALL       256    
#define MAX_NOTES_IN        128
#define MAX_MIDI_EVENTS_OUT 64
#define MAX_STEPS           65536

#define INVALID_STEP        MAX_STEPS

// Error Codes
#define ESTEPNOTPREPARED    -1
#define EMAXMIDIEVENTSOUT   -2
#define EMAXNOTESALL        -3
#define EORPHANEDNOTEOFF    -4


typedef struct Note_
{
    MidiEvent noteOn;
    uint32_t noteOnStep;
    MidiEvent noteOff;
    uint32_t noteOffStep;
    bool complete;
}Note;

enum SequencerState
{
    SEQUENCER_READY,
    SEQUENCER_INITIAL_RECORD,
    SEQUENCER_LOOP
};

// Constants
const uint32_t tickPeriod  = 2000; //us

// State Variables
volatile SequencerState sequencerState = SEQUENCER_READY;
Note notesAll[MAX_NOTES_ALL] = {}; // TODO: volatile
Note notesIn[MAX_NOTES_IN] = {};
MidiEvent midiEventsOut[MAX_MIDI_EVENTS_OUT] = {};
uint8_t numNotesAll               = 0;
uint8_t numNotesIn                = 0;
uint8_t numMidiEventsOut          = 0;
uint8_t numBeats                  = 8;
bool activeChannels[NUM_CHANNELS] = {};
volatile uint32_t numSteps        = 0;
volatile uint32_t curStep         = 0;

// Flags
volatile bool nextStepPreparedFlag = false;
volatile bool fireStepFlag = false;


static void Error(int errorCode)
{
    PrintFormat("Error: %d\n", errorCode);
    // TODO: Flash leds to signify error?
    // Or special midi signal?
}


static void ClearNotesIn()
{
    memset(notesIn, 0, sizeof(notesIn));
    numNotesIn = 0;
}


static void ClearNotesAll()
{
    memset(notesAll, 0, sizeof(notesAll));
    numNotesAll = 0;
}


static void MidiEventNoteIn(MidiEvent event, uint32_t step)
{
    if (event.type == MIDI_NOTE_ON)
    {
        if (numNotesIn <= MAX_NOTES_IN)
        {
            Note noteIn = 
            {
                event,
                step,
                {},
                step,  // Set to same as noteOnStep as placeholder
                false,
            };
            notesIn[numNotesIn] = noteIn;
            numNotesIn++;
        }
    }
    else if (event.type == MIDI_NOTE_OFF)
    {
        for (int i; i < numNotesIn; i++)
        {
            if (!notesIn[i].complete && notesIn[i].noteOn.value == event.value)
            {
                notesIn[i].noteOff = event;
                notesIn[i].noteOffStep = step;
                notesIn[i].complete = true;
                return;
            }
        }
        Error(EORPHANEDNOTEOFF);
    }
}


// TODO: data integrity check?
// orphaned events
// channels not matching in a note


static void StoreNotesIn()
{
    #ifdef DEBUG
    PrintFormat("Store notesIn\n");
    #endif
    if (numNotesAll + numNotesIn > MAX_NOTES_ALL)
    {
        Error(EMAXNOTESALL);
        return;
    }
    memcpy(&notesAll[numNotesAll], notesIn, numNotesIn * sizeof(notesIn[0]));
    numNotesAll += numNotesIn;
    ClearNotesIn();
}


static void Step()
{
    curStep = (curStep + 1) % numSteps;
    fireStepFlag = true;
    return;    
}


// Find all the midi events in notesAll which should be fired in the next step and 
// stage them in midiEventsOut, ready to be fired.
static void PrepareNextStep()
{
    // Only prepare the next step if it isn't prepared already.
    if (nextStepPreparedFlag)
    {
        return;
    }
    
    // Store the next step locally, wrapping around to zero if we've reached the last step.
    uint32_t nextStep = (curStep + 1) % numSteps;
   
    // Number of midi events staged for the next step.
    uint8_t midiEventsOutCounter = 0;
    
    // Clear down the list of staged midi events.
    memset(midiEventsOut, 0, sizeof(midiEventsOut));

    // Find midi events in notesAll which should be fired in the next step and stage them in midiEventsOut
    for (int note = 0; note < numNotesAll; note++)
    {
        // Check note on event for: active channel and step==nextStep
        if (activeChannels[notesAll[note].noteOn.channel] &&
            notesAll[note].noteOnStep == nextStep)
        {
            midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOn;
            midiEventsOutCounter++;
        }
        // Check note off event for: active channel and step==nextStep
        if (activeChannels[notesAll[note].noteOff.channel] &&
            notesAll[note].noteOffStep == nextStep)
        {
            midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOff;
            midiEventsOutCounter++;
        }
        // Ensure we don't overflow midiEventsOut
        if (midiEventsOutCounter >= MAX_MIDI_EVENTS_OUT)
        {
            Error(EMAXMIDIEVENTSOUT);
            break;
        }
    }
    
    numMidiEventsOut = midiEventsOutCounter;
    nextStepPreparedFlag = true;
}


// Output all the staged midiEvents for the current step.
static void FireCurrentStep()
{
    // Ensures step is not fired twice.
    if (!fireStepFlag)
    {
        return;
    }

    // Flash big button LED to indicate start of loop
    if (curStep == 0)
    {
        LedOn(1, 0);
    }
    if (curStep == 32)
    {
        LedOff(1, 0);
    }
    
    // if this step is prepared, then output all the midi events staged in midiEventsOut
    if (nextStepPreparedFlag)
    {         
        int localNumMidiEventsOut = numMidiEventsOut;
        
        for (int midiEvent = 0; midiEvent < localNumMidiEventsOut; midiEvent++)
        {
            ControllerOutputEvent(midiEventsOut[midiEvent]);
        }

        fireStepFlag = false;
    }
    else
    {
        Error(ESTEPNOTPREPARED);   
    }

    numMidiEventsOut = 0;    
    nextStepPreparedFlag = false;
}


static void StateTransition(SequencerState state)
{
    switch(state)
    {
        case SEQUENCER_READY:
        {
            #ifdef DEBUG
            PrintFormat("Sequencer ready\n");
            #endif
            sequencerState = SEQUENCER_READY;
            break;
        }
        case SEQUENCER_INITIAL_RECORD:
        {
            #ifdef DEBUG
            PrintFormat("Sequencer initial record\n");
            #endif
            sequencerState = SEQUENCER_INITIAL_RECORD;
            break;
        }
        case SEQUENCER_LOOP:
        {
            #ifdef DEBUG
            PrintFormat("Sequencer loop\n");
            #endif
            sequencerState = SEQUENCER_LOOP;
            break;
        }
        default:
            //todo Error()
            break;   
    }
}


static void JumpToStep(uint32_t st)
{
    nextStepPreparedFlag = false;
    curStep = abs(st - 1) % numSteps; // go to previous step to allow step to be prepared

    PrintFormat("Jumping to step %d\n", curStep);
}


static void ResetSequencer()
{
    Timer2.stop();
    ClearNotesIn();
    ClearNotesAll();
    curStep = 0;
    numSteps = 0;
    
    // initialise activeChannels
    for (int chan = 0; chan < NUM_CHANNELS; chan++)
    {
        activeChannels[chan] = true;
    }
    
    StateTransition(SEQUENCER_READY);
    ControllerChannelShift(NUM_CHANNELS * -1, false);
    // todo: refresh led output
}


// Deletes all notes for the given channel
static void ClearChannel(uint8_t channel)
{
    uint8_t localNumNotesAll = numNotesAll;
    
    // delete notes by moving elements from the end into their position
    // and decrementing numNotesAll
    #ifdef DEBUG
    PrintFormat("Clear down channel %d\n", channel);
    #endif
    for (int note = 0; note < localNumNotesAll; note++)
    {
        if (notesAll[note].noteOn.channel == channel)
        {
            notesAll[note--] = notesAll[numNotesAll - 1];
            localNumNotesAll--;
        }
    }

    numNotesAll = localNumNotesAll;
}


// Divides the loop up evenly and finds the closest point to given step
static uint32_t GetNearestQuantisePoint(uint32_t divisor, uint32_t step)
{
    uint32_t barLength = numSteps / divisor;

    uint32_t floor = step / barLength;
    uint32_t remainder = step % barLength;

    return remainder > barLength / 2 ? floor * barLength : (floor - 1) * barLength;  
}


// Mark a channel as active / inactive
void SequencerChannelOnOff(uint8_t channel, bool on)
{
    // todo: local activeChannels?
    if (activeChannels[channel] == on)
    {
        return;
    }
    
    activeChannels[channel] = on;
    ControllerOutputChannelOnOff(channel, activeChannels[channel]);

    // Prevent hanging notes if channel deactivated during a note
    if (!activeChannels[channel])
    {
        for (int note = 0; note < numNotesAll; note++)
        {
            if (notesAll[note].noteOn.channel == channel)
            {
                ControllerOutputEvent(notesAll[note].noteOff);
            }
        }
    }
    #ifdef DEBUG
    PrintFormat("Sequencer channel %d %s\n", channel, activeChannels[channel] ? "on" : "off");
    #endif
}


void SequencerChannelOnOffToggle(uint8_t channel)
{
    SequencerChannelOnOff(channel, !activeChannels[channel]);
}


void SequencerInputMidiEvent(MidiEvent midiEvent)
{
    uint32_t localCurStep = curStep;

    switch(sequencerState)
    {
        case SEQUENCER_READY:
            curStep = 0;
            numSteps = MAX_STEPS;
            ClearNotesIn();
            ClearNotesAll();
            MidiEventNoteIn(midiEvent, 0);
            Timer2.start();
            StateTransition(SEQUENCER_INITIAL_RECORD);
            break;
        case SEQUENCER_INITIAL_RECORD:
            MidiEventNoteIn(midiEvent, localCurStep);
            break;
        case SEQUENCER_LOOP:
            SequencerChannelOnOff(midiEvent.channel, false); // disable channel while writing over it
            MidiEventNoteIn(midiEvent, localCurStep);
            break;    
    }
}


void SequencerOkEvent()
{
    uint32_t localCurStep = curStep;
    
    switch(sequencerState)
    {
        case SEQUENCER_READY:
            // do nothing
            break;
        case SEQUENCER_INITIAL_RECORD:
            numSteps = localCurStep + 1;
            PrintFormat("numSteps: %d\n", numSteps);
            StoreNotesIn();
            nextStepPreparedFlag = false;
            StateTransition(SEQUENCER_LOOP);
            ControllerChannelShift(1, true);
            break;
        case SEQUENCER_LOOP:
            if (numNotesIn == 0)
            {
                JumpToStep(GetNearestQuantisePoint(16, localCurStep));
            }
            else
            {
                nextStepPreparedFlag = false;
                uint8_t channel = ControllerGetCurrentChannel();
                ClearChannel(channel);
                StoreNotesIn();
                SequencerChannelOnOff(channel, true);
                ControllerChannelShift(1, true);
            }
            break;    
    }
}


bool SequencerGetChannelOnOff(uint8_t channel)
{
    return activeChannels[channel];    
}


void SequencerClear(uint8_t channel)
{
    // todo: incorporate into state machine
    
    uint8_t localNumNotesIn = numNotesIn;

    if (localNumNotesIn == 0)
    {
        ClearChannel(channel);
        ControllerChannelShift(-1, false);
    }
    else 
    {
        ClearNotesIn();
    }
    
    if (numNotesAll == 0)
    {
        ResetSequencer();
    }
}


void SequencerReset()
{
    // todo: incorporate into state machine?
    ResetSequencer();
}


void SequencerBackgroundTasks()
{
    PrepareNextStep();
    FireCurrentStep();
}


void SequencerInit()
{
    Timer2.attachInterrupt(Step).setPeriod(tickPeriod);
    ResetSequencer();
}

