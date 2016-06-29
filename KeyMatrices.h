#pragma once

#include "Controller.h"
#include "KeyMatrixScanner.h"
#include "Sequencer.h"

#define PIANO_KEYS_IN_1        42
#define PIANO_KEYS_IN_2        40
#define PIANO_KEYS_OUT_A       38
#define PIANO_KEYS_OUT_B       36
#define PIANO_KEYS_OUT_C       34
#define PIANO_KEYS_OUT_D       32

#define JOYSTICK_IN_1          37
#define JOYSTICK_IN_2          41
#define JOYSTICK_IN_3          39
#define JOYSTICK_IN_4          35

#define MAIN_BUTTON_0          27
#define MAIN_BUTTON_1          29
#define MAIN_BUTTON_2          31
#define MAIN_BUTTON_3          33

#define PURPLE_BUTTON_IN       53
#define TURNTABLE_IN           25
#define ROLLER_IN              43

#define NUM_KEY_MATRICES       4

static const KeyMatrix pianoKeys = 
{
    {PIANO_KEYS_IN_1, PIANO_KEYS_IN_2},
    {PIANO_KEYS_OUT_A, PIANO_KEYS_OUT_B, PIANO_KEYS_OUT_C, PIANO_KEYS_OUT_D},
    2,
    4,
    8,
    {},
    {{7, 6, 5, 4}, {3, 2, 1, 0}},
    PianoKeyInput
};

static const KeyMatrix joystick = 
{
    {JOYSTICK_IN_1, JOYSTICK_IN_2, JOYSTICK_IN_3, JOYSTICK_IN_4},
    {GROUND},
    4,
    1,
    4,
    {},
    {{0}, {1}, {2}, {3}},
    JoystickInput
};

static const KeyMatrix mainButtons = 
{
    {MAIN_BUTTON_0, MAIN_BUTTON_1, MAIN_BUTTON_2, MAIN_BUTTON_3},
    {GROUND},
    4,
    1,
    4,
    {},
    {{0}, {1}, {2}, {3}},
    MainButtonsInput
};

static const KeyMatrix other = 
{
    {PURPLE_BUTTON_IN, TURNTABLE_IN, ROLLER_IN},
    {GROUND},
    3,
    1,
    3,
    {},
    {{0}, {1}, {2}},
    OtherButtonsInput   
};

static KeyMatrix keyMatrices[NUM_KEY_MATRICES] = 
{
    pianoKeys,
    joystick,
    mainButtons,
    other
};


