#include <stdint.h>

#include "DueTimer.h"
#include "Comms.h"
#include "Controller.h"
#include "Leds.h"
#include "Sequencer.h"

//#define DEBUG

#define MAX_NOTES_ALL          256
#define MAX_NOTES_IN           128
#define MAX_MIDI_EVENTS_OUT    64
#define MAX_STEPS              65536 //TODO: make this larger?
#define NUM_QUANTISE_DIVISORS  6
#define NO_QUANTISE            -1

#define INVALID_STEP        MAX_STEPS

// Error Codes
#define ESTEPNOTPREPARED    -1
#define EMAXMIDIEVENTSOUT   -2
#define EMAXNOTESALL        -3
#define EORPHANEDNOTEOFF    -4
#define ENOTEOUTOFRANGE     -5

typedef struct Note_
{
    MidiEvent noteOn;
    uint32_t noteOnStep;
    MidiEvent noteOff;
    uint32_t noteOffStep;
    bool complete;
    int32_t quantiseOffset;
}Note;

enum SequencerState
{
    SEQUENCER_READY,
    SEQUENCER_INITIAL_RECORD,
    SEQUENCER_LOOP
};

// Constants
const uint32_t tickPeriod  = 2000; //us
const int8_t quantiseDivisors[NUM_QUANTISE_DIVISORS] = {NO_QUANTISE, 16, 8, 4, 2, 1};


// State Variables
volatile SequencerState sequencerState = SEQUENCER_READY;
Note notesAll[MAX_NOTES_ALL] = {}; // TODO: volatile
Note notesIn[MAX_NOTES_IN] = {};
MidiEvent midiEventsOut[MAX_MIDI_EVENTS_OUT] = {};
uint8_t numNotesAll = 0;
uint8_t numNotesIn = 0;
uint8_t numMidiEventsOut = 0;
uint8_t numBeats = 8;
bool activeChannels[NUM_CHANNELS] = {};
uint32_t innerLoopLength = 0;
uint32_t channelNumInnerLoops[NUM_CHANNELS] = {};
uint32_t channelLoopOffset[NUM_CHANNELS] = {};
uint32_t channelQuantiseDivisorIndex[NUM_CHANNELS] = {};
volatile uint32_t numSteps = 0;
volatile uint32_t curStep = 0;

// Flags
volatile bool nextStepPreparedFlag = false;
volatile bool fireStepFlag = false;

static void PrintNote(Note * note)
{
    PrintFormat("channel: %d, value: %d, velocity: %d\n", note->noteOn.channel, note->noteOn.value, note->noteOn.velocity);
    PrintFormat("noteOnStep: %d\n", note->noteOnStep);
    PrintFormat("noteOffStep: %d\n", note->noteOffStep);
}


static void DumpState()
{
    PrintFormat("********DUMP STATE********\n\n\n"); // The extra \n's are padding to prevent an alignment error TODO: Get to the bottom of this!
    PrintFormat("Sequencer State: %d\n", sequencerState);
    PrintFormat("\n");
    PrintFormat("notesAll (%d):\n", numNotesAll);
    for (int i = 0; i < numNotesAll; i++)
    {
        PrintNote(&notesAll[i]);
    }
    PrintFormat("\n");
    PrintFormat("notesIn (%d):\n", numNotesIn);
    for (int i = 0; i < numNotesIn; i++)
    {
        PrintNote(&notesIn[i]);
    }
    PrintFormat("\n");
    PrintFormat("activeChannels:\n");
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        PrintFormat("%d: %s\n", i, activeChannels[i] ? "ON" : "OFF");
    }
    PrintFormat("innerLoopLength: %d\n", innerLoopLength);
    PrintFormat("channelNumInnerLoops:\n");
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        PrintFormat("%d: %d\n", i, channelNumInnerLoops[i]);
    }
    PrintFormat("channelLoopOffset:\n");
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        PrintFormat("%d: %d\n", i, channelLoopOffset[i]);
    }
    PrintFormat("curStep: %d\n", curStep);
    PrintFormat("numSteps: %d\n", numSteps);
    PrintFormat("********END DUMP********\n");
}


static void Error(int errorCode)
{
    PrintFormat("Error: %d\n", errorCode);
    DumpState();
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
                0,
            };
            notesIn[numNotesIn] = noteIn;
            numNotesIn++;
        }
    }
    else if (event.type == MIDI_NOTE_OFF)
    {
        for (int i = 0; i < numNotesIn; i++)
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


static uint32_t GetMaxNumInnerLoops()
{
    uint32_t maxNumInnerLoops = 0;

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        if (channelNumInnerLoops[i] > maxNumInnerLoops)
        {
            maxNumInnerLoops = channelNumInnerLoops[i];
        }
    }

    return maxNumInnerLoops;
}


static uint32_t DeduplicateMidiEvents(MidiEvent * events, uint32_t numEvents)
{
    // TODO: get rid of the noteOff events too
    if (numEvents > 1)
    {
       PrintFormat("duplicates\n");
    }
    for (int i = 0; i < numEvents; i++)
    {
        for (int j = i + 1; j < numEvents; j++)
        {
            // If there's a duplicate
            if (events[i].value == events[j].value && events[i].channel == events[i].channel)
            {
                // Then delete it by shifting the rest of the entries down one
                for (int k = j; k < numEvents; k++)
                {
                   events[k] = events[k + 1];
                }
                numEvents--;
            }
        }
    }
    return numEvents;
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

    if (nextStep % innerLoopLength == 0)
    {
        uint32_t curLoop = nextStep / innerLoopLength;
        uint32_t numLoopsTotal = numSteps / innerLoopLength;

        for (int chan = 0; chan < NUM_CHANNELS; chan++)
        {
            uint32_t loopOffset = channelLoopOffset[chan];
            uint32_t numInnerLoops = channelNumInnerLoops[chan];

            if ((loopOffset + numInnerLoops) % numLoopsTotal == curLoop)
            {
                loopOffset += numInnerLoops;
                PrintFormat("Chan %d,loopOffset: %d\n", chan, loopOffset % (numSteps / innerLoopLength));
            }
            loopOffset = loopOffset % (numSteps / innerLoopLength);

            channelLoopOffset[chan] = loopOffset;
            channelNumInnerLoops[chan] = numInnerLoops;
        }
    }

    // Find midi events in notesAll which should be fired in the next step and stage them in midiEventsOut
    for (int note = 0; note < numNotesAll; note++)
    {
        uint8_t channel = notesAll[note].noteOn.channel;

        // Check note on event for active channel
        if (activeChannels[channel])
        {
            // loopOffset and quantiseOffset
            uint32_t noteOffset = channelLoopOffset[channel] * innerLoopLength + notesAll[note].quantiseOffset;

            // Should noteOn be fired this step?
            if (nextStep == (notesAll[note].noteOnStep + noteOffset) % numSteps)
            {
                midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOn;
                midiEventsOutCounter++;
            }

            // Should noteOff be fired this step?
            if (nextStep == (notesAll[note].noteOffStep + noteOffset) % numSteps)
            {
                midiEventsOut[midiEventsOutCounter] = notesAll[note].noteOff;
                midiEventsOutCounter++;
            }
        }

        // Ensure we don't overflow midiEventsOut
        if (midiEventsOutCounter >= MAX_MIDI_EVENTS_OUT)
        {
            Error(EMAXMIDIEVENTSOUT);
            break;
        }
    }

    numMidiEventsOut = DeduplicateMidiEvents(midiEventsOut, midiEventsOutCounter);
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
    innerLoopLength = 0;
    curStep = 0;
    numSteps = 0;

    // initialise activeChannels
    for (int chan = 0; chan < NUM_CHANNELS; chan++)
    {
        activeChannels[chan] = true;
        channelNumInnerLoops[chan] = 0;
        channelLoopOffset[chan] = 0;
    }

    ControllerChannelShift(NUM_CHANNELS * -1, false);
    // TODO: refresh led output
}


// Deletes all notes for the given channel
static void ClearChannelNotes(uint8_t channel)
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

    channelNumInnerLoops[channel] = 0;
    channelLoopOffset[channel] = 0;
}


// Divides the steps between start and step into quanta and finds the closest quantise point to step
static uint32_t GetNearestQuantisePoint(uint32_t start, uint32_t quantaLength, uint32_t step)
{
    // TODO: check out of range and step > start
    uint32_t floor = (step - start) / quantaLength;
    uint32_t remainder = (step - start) % quantaLength;

    return remainder > quantaLength / 2 ? (floor + 1) * quantaLength : floor * quantaLength;  
}


// Divides the steps between start and step up into quanta and finds the next quantise point to step
static uint32_t GetNextQuantisePoint(uint32_t start, uint32_t quantaLength, uint32_t step)
{
    // TODO: check out of range and step > start
    uint32_t floor = (step - start) / quantaLength;

    return (floor + 1) * quantaLength;
}


// Divides steps between start and step up into quanta and finds the previous quantise point to step
static uint32_t GetPrevQuantisePoint(uint32_t start, uint32_t quantaLength, uint32_t step)
{
    // TODO: check out of range and step > start
    uint32_t floor = (step - start) / quantaLength;

    return floor * quantaLength;
}


// Shift a note a number of steps back or forth
// NB: output pointer could be the same as input
static void ShiftNote(Note * output, Note * input, uint32_t stepsToShift)
{
    uint32_t noteOnStep = input->noteOnStep + stepsToShift;
    uint32_t noteOffStep = input->noteOffStep + stepsToShift;

    if (noteOnStep < 0 || noteOnStep >= MAX_STEPS)
    {
        Error(ENOTEOUTOFRANGE);
        return;
    }

    if (noteOffStep < 0 || noteOffStep >= MAX_STEPS)
    {
        Error(ENOTEOUTOFRANGE);
        return;
    }

    output->noteOnStep = noteOnStep;
    output->noteOffStep = noteOffStep;
}


// Shift all the notes in notesIn such that the first note starts in the first inner loop
static void ShiftNotesInToStart(uint32_t numInnerLoops)
{
    if (notesIn[0].noteOnStep >= innerLoopLength)
    {
        uint32_t numLoopsToShift = notesIn[0].noteOnStep / innerLoopLength;

        for (int i = 0; i < numNotesIn; i++)
        {
            ShiftNote(&notesIn[i], &notesIn[i], numLoopsToShift * innerLoopLength * -1);
        }
    }
}


// Shift notes that fall off the end of the loop to the start
static void WrapNotesAround(uint32_t numInnerLoops)
{
    for (int i = 0; i < numNotesIn; i++)
    {
        if (notesIn[i].noteOnStep >= innerLoopLength * numInnerLoops)
        {
            ShiftNote(&notesIn[i], &notesIn[i], numInnerLoops * innerLoopLength * -1);
        }
    }
}


static void ApplyQuantisation(uint8_t channel)
{
    uint8_t quantiseDivisorIndex = (channelQuantiseDivisorIndex[channel] + 1) % NUM_QUANTISE_DIVISORS;

    for (int i = 0; i < numNotesAll; i++)
    {
        if (notesAll[i].noteOn.channel == channel)
        {
            if (quantiseDivisors[quantiseDivisorIndex] == NO_QUANTISE)
            {
                notesAll[i].quantiseOffset = 0;
            }
            else
            {
                uint32_t numInnerLoops = channelNumInnerLoops[channel];
                uint32_t quantaLength = (numInnerLoops * innerLoopLength) / quantiseDivisors[quantiseDivisorIndex];
                uint32_t quantisedStep = GetNearestQuantisePoint(0, quantaLength, notesAll[i].noteOnStep);
                notesAll[i].quantiseOffset = (int32_t)quantisedStep - (int32_t)notesAll[i].noteOnStep;
            }
        }
    }
    if (quantiseDivisorIndex == 0)
    {
        for (int i = 0; i < 8; i++)
        {
            LedFlash(0, i, 3);
        }
    }
    channelQuantiseDivisorIndex[channel] = quantiseDivisorIndex;
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


void SequencerDumpState()
{
    DumpState();
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
            // Extend numSteps to the largest allowed multiple of numSteps
            numSteps = (MAX_STEPS / numSteps) * numSteps;
            // disable channel while writing over it
            SequencerChannelOnOff(midiEvent.channel, false);
            // Record midi event
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
            innerLoopLength = numSteps;
            channelNumInnerLoops[0] = 1;
            PrintFormat("numSteps: %d\n", numSteps);
            StoreNotesIn();
            nextStepPreparedFlag = false;
            ControllerChannelShift(1, true);
            StateTransition(SEQUENCER_LOOP);
            break;
        case SEQUENCER_LOOP:
            if (numNotesIn == 0)
            {
                ControllerChannelShift(1, true);
            }
            else
            {
                uint8_t channel = ControllerGetCurrentChannel();
                // Write over whatever was stored in this channel previously
                ClearChannelNotes(channel);
                // Get the loop length from first noteOn to now.
                uint32_t numInnerLoops = GetNearestQuantisePoint(notesIn[0].noteOnStep, innerLoopLength, localCurStep) / innerLoopLength;
                // Min number of loops is 1
                if(numInnerLoops == 0)
                {
                    numInnerLoops = 1;
                }
                // Shift notes so that start of loop is step 0 and use loop offset to compensate for the shift
                uint32_t loopOffset = (notesIn[0].noteOnStep / innerLoopLength);
                ShiftNotesInToStart(numInnerLoops);
                // Some notes may have been chopped off by GetNearestQuantisePoint() these should be wrapped around to the start of the loop
                WrapNotesAround(numInnerLoops);
                channelNumInnerLoops[channel] = numInnerLoops;
                uint32_t maxNumInnerLoops = GetMaxNumInnerLoops();
                channelLoopOffset[channel] = loopOffset % maxNumInnerLoops;
                numSteps = maxNumInnerLoops * innerLoopLength;
                StoreNotesIn();
                SequencerChannelOnOff(channel, true);
                ControllerChannelShift(1, true);
            }
            break;
    }
}


void SequencerClearEvent(uint8_t channel)
{
    uint32_t localCurStep = curStep;
    uint32_t localNumSteps = numSteps;
    uint32_t localNumNotesIn = numNotesIn;

    switch(sequencerState)
    {
        case SEQUENCER_READY:
            // do nothing
            break;
        case SEQUENCER_INITIAL_RECORD:
            ResetSequencer();
            StateTransition(SEQUENCER_READY);
            break;
        case SEQUENCER_LOOP:
            if (localNumNotesIn == 0)
            {
                ControllerChannelShift(-1, false);
            }
            else
            {
                ClearNotesIn();
                // Readjust curSteps and numSteps to reflect clear.
                localNumSteps = innerLoopLength * GetMaxNumInnerLoops();
                localCurStep = localCurStep % (innerLoopLength * GetMaxNumInnerLoops());
            }
            break;
    }

    numSteps = localNumSteps;
    curStep = localCurStep;
}


void SequencerResetEvent()
{
    switch(sequencerState)
    {
        case SEQUENCER_READY:
        case SEQUENCER_INITIAL_RECORD:
        case SEQUENCER_LOOP:
            ResetSequencer();
            StateTransition(SEQUENCER_READY);
            break;
    }
}


void SequencerQuantise(uint8_t channel)
{
    ApplyQuantisation(channel);
}


bool SequencerGetChannelOnOff(uint8_t channel)
{
    return activeChannels[channel];
}


void SequencerBackgroundTasks()
{
    PrepareNextStep();
    FireCurrentStep();
}


void SequencerInit()
{
    Timer2.attachInterrupt(Step).setPeriod(tickPeriod);
    SequencerResetEvent();
}
