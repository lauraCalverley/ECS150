#include "VirtualMachine.h"
#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

extern "C" {
using namespace std;

    TVMMainEntry VMLoadModule(const char *module);

    
    TVMStatus VMStart(int tickms, int argc, char *argv[]) {
        TVMMainEntry module = VMLoadModule(argv[0]);
        if (module == NULL) {
            return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
        }
        else {
            module(argc, argv);
            return VM_STATUS_SUCCESS;
        }
    }
    
    
    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

        if ((data==NULL) || (length==NULL)) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        int writeStatus;
        writeStatus = write(filedescriptor, data, *length); // FIXME - temporary solution
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
            //MachineRequestAlarm(tick, TMachineAlarmCallback callback, void *calldata);
            // Upon successful sleep of the currently running thread, VMThreadSleep() returns VM_STATUS_SUCCESS.
            
        }
    }
    

    
}