#include <stdint.h>

#include "DueTimer.h"

#include "Comms.h"
#include "Controller.h"
#include "Leds.h"
#include "Sequencer.h"

//#define DEBUG

#define MAX_ALL_NOTES       256    
#define MAX_NOTES_IN        128
#define MAX_MIDI_EVENTS_OUT 64

#define NUM_STEPS           1024
#define MIN_TICK_PERIOD     1000    //TODO: use this limit

#define INVALID_STEP        -1

#define CLICK_NOTE_VALUE    65
#define CLICK_CHANNEL       7

// error codes
#define ESTEPNOTPREPARED    -1
#define EMAXMIDIEVENTS      -2
#define EORPHANEDNOTEOFF    -3

typedef struct Note_
{
    MidiEvent noteOn;
    uint16_t noteOnStep;
    MidiEvent noteOff;
    uint16_t noteOffStep;
    bool complete;
}Note;

uint8_t numBeats        =   8;
uint32_t tickPeriod     =   4000; //us

Note allNotes[MAX_ALL_NOTES] = {}; // TODO: volatile
Note notesIn[MAX_NOTES_IN] = {};
MidiEvent midiEventsOut[MAX_MIDI_EVENTS_OUT] = {};
uint8_t numAllNotes = 0;
uint8_t numNotesIn = 0;
uint8_t numMidiEventsOut = 0;

bool activeChannels[NUM_CHANNELS] = {};

volatile uint16_t curStep = 0;
bool nextStepPrepared = false;

bool running = false;

uint16_t lastTapTempoStep = 0;
bool tapTempoEnable = false;

volatile bool fireStepTrigger = false;


void Error(int errorCode)
{
    PrintFormat("Error: %d\n", errorCode);
}


void SequencerInputEvent(MidiEvent event)
{
    if(!running)
    {
        return;
    }
    
    uint32_t localCurStep = curStep;

    if (event.type == MIDI_NOTE_ON)
    {
        if (numNotesIn <= MAX_NOTES_IN)
        {
            Note noteIn = 
            {
                event,
                localCurStep,
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
                notesIn[i].noteOffStep = localCurStep;
                notesIn[i].complete = true;
                return;
            }
        }
        Error(EORPHANEDNOTEOFF);
    }
}


void SequencerDeactivateNote(uint8_t key)
{
    // TODO
}


void SequencerChannelOnOff(uint8_t channel)
{
    activeChannels[channel] = !activeChannels[channel];
    ControllerOutputChannelOnOff(channel, activeChannels[channel]);

    // Prevent hanging notes if channel deactivated during a note
    if (!activeChannels[channel])
    {
        for (int note = 0; note < numAllNotes; note++)
        {
            if (allNotes[note].noteOn.channel == channel)
            {
                ControllerOutputEvent(allNotes[note].noteOff);
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


//todo: data integrity check?
// orphaned events
// channels not matching in a note


void SequencerTapTempo()
{
    if (!tapTempoEnable)
    {
        tapTempoEnable = true;
    }
    else
    {
        tickPeriod = ((curStep - lastTapTempoStep) * tickPeriod) / 128;
        Timer3.setPeriod(tickPeriod);
    }

    #ifdef DEBUG
    PrintFormat("TapTempo: curStep: %d, lastTapTempoStep: %d, tickPeriod: %d\n", curStep, lastTapTempoStep, tickPeriod);
    #endif

    lastTapTempoStep = curStep;
}


void SequencerStartStop()
{
    running = !running;
    if(running)
    {
        #ifdef DEBUG
        PrintFormat("Sequencer start\n");
        #endif
        Timer2.start();
    }
    else
    {
        #ifdef DEBUG
        PrintFormat("Sequencer stop\n");
        #endif
        Timer2.stop();
    }
}


static void ClearNotesIn()
{
    memset(notesIn, 0, sizeof(notesIn));
    numNotesIn = 0;
}


void SequencerStoreLastCycle()
{
    #ifdef DEBUG
    PrintFormat("Store last cycle\n");
    #endif
    // TODO: check that allnotes will not overflow
    memcpy(&allNotes[numAllNotes], notesIn, numNotesIn * sizeof(notesIn[0]));
    numAllNotes += numNotesIn;
    ClearNotesIn();
}


// Deletes all notes for the given channel
void SequencerClearChannel(uint8_t channel)
{
    // delete notes by moving elements from the end into their position
    // and decrementing numAllNotes
    #ifdef DEBUG
    PrintFormat("Clear down channel %d\n", channel);
    #endif
    for (int note = 0; note < numAllNotes; note++)
    {
        if (allNotes[note].noteOn.channel == channel)
        {
            allNotes[note--] = allNotes[numAllNotes - 1];
            numAllNotes--;
        }
    }
}


static void Step()
{
    curStep = (curStep + 1) % NUM_STEPS;
    fireStepTrigger = true;
    return;    
}


static void PrepareNextStep()
{
    if (nextStepPrepared)
    {
        return;
    }
    
    uint16_t nextStep = (curStep + 1) % NUM_STEPS;    // use a local copy in case curStep value changes
    uint8_t midiEventsOutCounter = 0;
   
    memset(midiEventsOut, 0, sizeof(midiEventsOut));

    for (int note = 0; note < numAllNotes; note++)
    {
        if (allNotes[note].noteOnStep == nextStep &&
            activeChannels[allNotes[note].noteOn.channel])
        {
            midiEventsOut[midiEventsOutCounter] = allNotes[note].noteOn;
            midiEventsOutCounter++;
        }
        if (allNotes[note].noteOffStep == nextStep &&
            activeChannels[allNotes[note].noteOff.channel])
        {
            midiEventsOut[midiEventsOutCounter] = allNotes[note].noteOff;
            midiEventsOutCounter++;
        }
        if (midiEventsOutCounter >= MAX_MIDI_EVENTS_OUT)
        {
            Error(EMAXMIDIEVENTS);
            break;
        }
    }

    numMidiEventsOut = midiEventsOutCounter;
    nextStepPrepared = true;
}


void FireCurrentStep()
{
    if (!fireStepTrigger)
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
    
    if (nextStepPrepared)
    {         
        int localNumMidiEventsOut = numMidiEventsOut;
        
        for (int midiEvent = 0; midiEvent < localNumMidiEventsOut; midiEvent++)
        {
            ControllerOutputEvent(midiEventsOut[midiEvent]);
        }

        fireStepTrigger = false;
    }
    else
    {
        Error(ESTEPNOTPREPARED);   
    }

    numMidiEventsOut = 0;    
    nextStepPrepared = false;
}


void SequencerBackgroundTasks()
{
    PrepareNextStep();
    FireCurrentStep();
    // todo: manage notesIn
}


void AddClickTrack()
{
    MidiEvent clickOnBeat0 = BuildMidiEvent(MIDI_NOTE_ON, CLICK_CHANNEL, CLICK_NOTE_VALUE, 126);
    MidiEvent clickOn = BuildMidiEvent(MIDI_NOTE_ON, CLICK_CHANNEL, CLICK_NOTE_VALUE, 90);
    MidiEvent clickOff = BuildMidiEvent(MIDI_NOTE_OFF, CLICK_CHANNEL, CLICK_NOTE_VALUE, 0);
    for (int beat = 0; beat < numBeats; beat++)
    {   
        MidiEvent on = beat == 0 ? clickOnBeat0 : clickOn;
        Note clickNote = 
        {
            on,
            beat * NUM_STEPS / numBeats,
            clickOff,
            beat * NUM_STEPS / numBeats + 8,
            true
        };
        allNotes[numAllNotes++] = clickNote;
    }
}


void SequencerInit()
{
    // initialise activeChannels
    for (int chan = 0; chan < NUM_CHANNELS; chan++)
    {
        activeChannels[chan] = true;
    }

    AddClickTrack();

    Timer2.attachInterrupt(Step).setPeriod(tickPeriod);
}

