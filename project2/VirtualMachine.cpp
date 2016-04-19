#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {
    using namespace std;
    
    volatile int SLEEPCOUNT = 0;

    TVMMainEntry VMLoadModule(const char *module);
    TMachineAlarmCallback callback(void);
    
    TVMStatus VMStart(int tickms, int argc, char *argv[]) {
        MachineInitialize();
        
        //TMachineAlarmCallback (*callbackFunctionPointer)();
        //callbackFunctionPointer = &callback;
        //TMachineAlarmCallback callbackFunctionPointer = &callback();
        //MachineRequestAlarm(100*1000, callbackFunctionPointer, NULL); // 2nd arg is a function pointer

        TVMMainEntry module = VMLoadModule(argv[0]);
        if (module == NULL) {
            return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
        }
        else {
            module(argc, argv);
            return VM_STATUS_SUCCESS;
        }
    }
    
    TMachineAlarmCallback callback(void) {
            SLEEPCOUNT--;
        // default tick time is 100ms
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
        else if (tick = VM_TIMEOUT_IMMEDIATE) {
            // FIXME? No idea if this is right
            // If tick is specified as VM_TIMEOUT_IMMEDIATE the current process yields the remainder of its processing quantum to the next ready process of equal priority.
            return VM_STATUS_SUCCESS;
        }
        else {
            // puts the currently running thread to sleep for tick ticks
            // Upon successful sleep of the currently running thread, VMThreadSleep() returns VM_STATUS_SUCCESS.
            
            SLEEPCOUNT = tick; // set global
            
            while (SLEEPCOUNT != 0) {}

            if (SLEEPCOUNT == 0) {
                return VM_STATUS_SUCCESS;
            }
            
        }
    }
    

    
}