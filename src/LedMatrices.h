#pragma once

#define PIANO_KEY_LED_MATRIX     0
#define BIG_BUTTON_LED_MATRIX    1

#define PIANO_KEY_LED_0          32
#define PIANO_KEY_LED_1          30
#define PIANO_KEY_LED_2          28
#define PIANO_KEY_LED_3          26

#define PIANO_KEY_LED_A          24
#define PIANO_KEY_LED_B          22

#define BIG_BUTTON_LED_B         52

#define NUM_LED_MATRICES         2


static const LedMatrix pianoKeyLeds = 
{
    {PIANO_KEY_LED_0, PIANO_KEY_LED_1, PIANO_KEY_LED_2, PIANO_KEY_LED_3},
    {PIANO_KEY_LED_A, PIANO_KEY_LED_B},
    4,
    2,
    8,
    {},
    {},
    0,
    0,
    {{0, 4}, {1, 5}, {2, 6}, {3, 7}},
};

static const LedMatrix bigButtonLed = 
{
    {VCC_PIN},
    {BIG_BUTTON_LED_B},
    1,
    1,
    2,
    {},
    {},
    0,
    0,
    {0}, 
};

LedMatrix ledMatrices[] = {pianoKeyLeds, bigButtonLed};
