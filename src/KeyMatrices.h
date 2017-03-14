#pragma once

#include "Controller.h"
#include "KeyMatrixScanner.h"
#include "Sequencer.h"


#define PIANO_KEYS_IN_1        38
#define PIANO_KEYS_IN_2        40
#define PIANO_KEYS_OUT_A       42
#define PIANO_KEYS_OUT_B       44
#define PIANO_KEYS_OUT_C       46
#define PIANO_KEYS_OUT_D       48

#define JOYSTICK_IN_1          43
#define JOYSTICK_IN_2          41
#define JOYSTICK_IN_3          39
#define JOYSTICK_IN_4          37

#define MAIN_BUTTON_0          33
#define MAIN_BUTTON_1          31
#define MAIN_BUTTON_2          25
#define MAIN_BUTTON_3          23

#define PURPLE_BUTTON_IN       35
#define TURNTABLE_IN           49
#define ROLLER_IN              53


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
    {GROUND_PIN},
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
    {GROUND_PIN},
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
    other,
};