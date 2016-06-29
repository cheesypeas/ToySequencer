#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

#include "Comms.h"
#include "Controller.h"
#include "KeyMatrixScanner.h"
#include "Leds.h"
#include "Sequencer.h"


extern char _end;
extern "C" char *sbrk(int i);
char *ramstart=(char *)0x20070000;
char *ramend=(char *)0x20088000;


void PrintMemUsage() {
    char *heapend=sbrk(0);
    register char * stack_ptr asm ("sp");
    struct mallinfo mi=mallinfo();
    printf("\nDynamic ram used: %d\n",mi.uordblks);
    printf("Program static ram used %d\n",&_end - ramstart);
    printf("Stack ram used %d\n\n",ramend - stack_ptr);
    printf("My guess at free mem: %d\n",stack_ptr - heapend + mi.fordblks);
    printf("\n");
}


void StartupSequence()
{
    for (int i = 0; i < 8; i++)
    {
        LedOnOff(0, i, true);
        LedOnOff(0, 7 - i, true);
        if(i != 3)
        {
            delay(100);
        }
        LedOnOff(0, i, false);
        LedOnOff(0, 7 - i, false);
    }
}


void setup()
{  
    CommsInit();
    ControllerInit();
    KeyMatricesInit();
    SequencerInit();
    LedsInit();

    PrintMemUsage();
    
    StartupSequence();
    SequencerStartStop();
}


void loop()
{
    KeyMatrixCallbacks();
    SequencerBackgroundTasks();
}
