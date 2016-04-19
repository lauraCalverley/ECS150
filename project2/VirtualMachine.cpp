#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {
    using namespace std;
    
    volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values

    TVMMainEntry VMLoadModule(const char *module);
    void callbackMachineRequestAlarm(void *calldata);
    
    // The following are defined in VirtualMachineUtils.c:
    // TVMMainEntry VMLoadModule(const char *module)
    // void VMUnloadModule(void)
    // TVMStatus VMFilePrint(int filedescriptor, const char *format, ...)

    
    TVMStatus VMStart(int tickms, int argc, char *argv[]) {
        MachineInitialize();
        
        MachineRequestAlarm(100*1000, callbackMachineRequestAlarm, NULL); // 2nd arg is a function pointer

        TVMMainEntry module = VMLoadModule(argv[0]);
        if (module == NULL) {
            return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
        }
        else {
            module(argc, argv);
            return VM_STATUS_SUCCESS;
        }
    }
    
    void callbackMachineRequestAlarm(void *calldata) {
        SLEEPCOUNT--;
    }
    
    
    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

        if ((data==NULL) || (length==NULL)) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        int writeStatus;
        writeStatus = write(filedescriptor, data, *length); // FIXME - temporary solution // eventually use MachineFileWrite
        if (writeStatus < 0) {
            return VM_STATUS_FAILURE;
        }
        else {
            return VM_STATUS_SUCCESS;
        }
    }
    
    TVMStatus VMThreadSleep(TVMTick tick) {
        
        if (tick == VM_TIMEOUT_INFINITE) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        else if (tick == VM_TIMEOUT_IMMEDIATE) {
            // FIXME? No idea if this is right
            // If tick is specified as VM_TIMEOUT_IMMEDIATE the current process yields the remainder of its processing quantum to the next ready process of equal priority.
            return VM_STATUS_SUCCESS;
        }
        else {
            SLEEPCOUNT = tick; // set global
            while (SLEEPCOUNT != 0) {}
            return VM_STATUS_SUCCESS;
        }
    }
    
    
    
    // VMFileOpen
    // VMFileSeek
    // VMFileRead
    // VM FileClose
    
    
    
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 */

/*
 Pseudocode for Scheduling Algorithm
 
 
*/