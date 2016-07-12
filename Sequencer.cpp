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

#define MIN_TICK_PERIOD     1000    //TODO: use this limit

#define INVALID_STEP        -1

#define CLICK_NOTE_VALUE    65
#define CLICK_CHANNEL       7

// Error Codes
#define ESTEPNOTPREPARED    -1
#define EMAXMIDIEVENTSOUT   -2
#define EMAXNOTESALL        -3
#define EORPHANEDNOTEOFF    -4


typedef struct Note_
{
    MidiEvent noteOn;
    uint16_t noteOnStep;
    MidiEvent noteOff;
    uint16_t noteOffStep;
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
volatile uint16_t curStep         = 0;

// Flags
volatile bool nextStepPreparedFlag = false;
volatile bool fireStepFlag = false;


static void Error(int errorCode)
{
    PrintFormat("Error: %d\n", errorCode);
    // todo: Flash leds to signify error
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


static void MidiEventIn(MidiEvent event, uint32_t step)
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
                INVALID_STEP,
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


//todo: data integrity check?
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


static void PrepareNextStep()
{
    if (nextStepPreparedFlag)
    {
        return;
    }
    
    uint16_t nextStep = (curStep + 1) % numSteps;    // use a local copy in case curStep value changes
    uint8_t midiEventsOutCounter = 0;
   
    memset(midiEventsOut, 0, sizeof(midiEventsOut));

    for (int note = 0; note < numNotesAll; note++)
    {
        if (notesAll[note].noteOnStep == nextStep &&
            activeChannels[notesAll[note].noteOn.channel])
        {
            midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOn;
            midiEventsOutCounter++;
        }
        if (notesAll[note].noteOffStep == nextStep &&
            activeChannels[notesAll[note].noteOff.channel])
        {
            midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOff;
            midiEventsOutCounter++;
        }
        if (midiEventsOutCounter >= MAX_MIDI_EVENTS_OUT)
        {
            Error(EMAXMIDIEVENTSOUT);
            break;
        }
    }

    numMidiEventsOut = midiEventsOutCounter;
    nextStepPreparedFlag = true;
}


void FireCurrentStep()
{
    if (!fireStepFlag)
    {
        return;
    }

    if (curStep == 0)
    {
        LedOn(1, 0);
    }

    if (curStep == 32)
    {
        LedOff(1, 0);
    }
    
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
            MidiEventIn(midiEvent, 0);
            Timer2.start();
            StateTransition(SEQUENCER_INITIAL_RECORD);
            break;
        case SEQUENCER_INITIAL_RECORD:
            MidiEventIn(midiEvent, localCurStep);
            break;
        case SEQUENCER_LOOP:
            MidiEventIn(midiEvent, localCurStep);
            break;    
    }
}


void SequencerLoopEvent()
{
    uint32_t localCurStep = curStep;
    
    switch(sequencerState)
    {
        case SEQUENCER_READY:
            // do nothing
            break;
        case SEQUENCER_INITIAL_RECORD:
            numSteps = localCurStep + 1;
            StoreNotesIn();
            nextStepPreparedFlag = false;
            StateTransition(SEQUENCER_LOOP);
            break;
        case SEQUENCER_LOOP:
            nextStepPreparedFlag = false;
            numSteps = localCurStep + 1;
            break;    
    }
}


void SequencerChannelOnOff(uint8_t channel)
{
    activeChannels[channel] = !activeChannels[channel];
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


bool SequencerGetChannelOnOff(uint8_t channel)
{
    return activeChannels[channel];    
}


void ResetSequencer()
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
}


// Deletes all notes for the given channel
// If there are no more notes afterwards, reset.
void SequencerClearChannel(uint8_t channel)
{
    // delete notes by moving elements from the end into their position
    // and decrementing numNotesAll
    #ifdef DEBUG
    PrintFormat("Clear down channel %d\n", channel);
    #endif
    for (int note = 0; note < numNotesAll; note++)
    {
        if (notesAll[note].noteOn.channel == channel)
        {
            notesAll[note--] = notesAll[numNotesAll - 1];
            numNotesAll--;
        }
    }
    if (numNotesAll == 0)
    {
        ResetSequencer();
    }
}


static ManageNotesIn()
{
    //uint32_t localCurStep = curStep;
    
    //if (localCurStep == 0)
    //{
    //    ClearNotesIn()
    //}
}


void SequencerBackgroundTasks()
{
    PrepareNextStep();
    FireCurrentStep();
    ManageNotesIn();
}


void SequencerInit()
{
    Timer2.attachInterrupt(Step).setPeriod(tickPeriod);
    ResetSequencer();
}

