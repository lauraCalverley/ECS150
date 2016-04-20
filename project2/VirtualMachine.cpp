#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {
    using namespace std;
    
    volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values
    volatile int MACHINE_FILE_OPEN_STATUS = 0;
    volatile int MACHINE_FILE_SEEK_STATUS = 0;

    TVMMainEntry VMLoadModule(const char *module);
    void callbackMachineRequestAlarm(void *calldata);
    void callbackMachineFileOpen(void *calldata, int result);
    void callbackMachineFileSeek(void *calldata, int result);
    
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
    
    TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
        
        if ((filename==NULL) || (filedescriptor==NULL)) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        MachineFileOpen(filename, flags, mode, callbackMachineFileOpen, filedescriptor);
        
        while (MACHINE_FILE_OPEN_STATUS != 1) {} // FIXME Multi When a thread calls VMFileOpen() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful opening of the file is completed.
        MACHINE_FILE_OPEN_STATUS = 0; // reset

        if (*filedescriptor < 0) {
            return VM_STATUS_FAILURE;
        }
        else {
            return VM_STATUS_SUCCESS;
        }
    }
    
    void callbackMachineFileOpen(void *calldata, int result) {
        // result is the File Descriptor, which is the result of MachineFileOpen
        *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
        MACHINE_FILE_OPEN_STATUS = 1;
    }
    
    
    // VMFileSeek
    
    TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
        
        MachineFileSeek(filedescriptor, offset, whence, callbackMachineFileSeek, newoffset);
        
        while (MACHINE_FILE_SEEK_STATUS != 1) {} // FIXME Multi When a thread calls VMFileSeek() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful seeking in the file is completed.
        MACHINE_FILE_SEEK_STATUS = 0; // reset
        
        if (*newoffset < 0) {
            return VM_STATUS_FAILURE;
        }
        else {
            return VM_STATUS_SUCCESS;
        }
    }
    
    void callbackMachineFileSeek(void *calldata, int result) {
        // result is the File Descriptor, which is the result of MachineFileOpen
        *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
        MACHINE_FILE_SEEK_STATUS = 1;
    }

    
    // VMFileRead
    TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
        
        /*
         VMFileRead() attempts to read the number of bytes specified in the integer referenced by length into the location specified by data from the file specified by filedescriptor. The filedescriptor should have been obtained by a previous call to VMFileOpen(). The actual number of bytes transferred by the read will be updated in the length location. When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
         */
        
        
    }
    
    
    // VMFileClose
    
    
    
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 */

/*
 Pseudocode for Scheduling Algorithm
 
 
*/