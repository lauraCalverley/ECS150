#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"

extern "C" {
using namespace std;

volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values
volatile int MACHINE_FILE_OPEN_STATUS = 0;
volatile int MACHINE_FILE_SEEK_STATUS = 0;
volatile int MACHINE_FILE_READ_STATUS = 0;
volatile int MACHINE_FILE_WRITE_STATUS = 0;
volatile int MACHINE_FILE_CLOSE_STATUS = 0;

TVMMainEntry VMLoadModule(const char *module);
void callbackMachineRequestAlarm(void *calldata);
void callbackMachineFileOpen(void *calldata, int result);
void callbackMachineFileSeek(void *calldata, int result);
void callbackMachineFileRead(void *calldata, int result);
void callbackMachineFileWrite(void *calldata, int result);
void callbackMachineFileClose(void *calldata, int result);

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
        //TVMThreadID VMMainThreadID;
        //VMThreadCreate(module, NULL, 0x100000, VM_THREAD_PRIORITY_NORMAL, &VMMainThreadID);
        //VMThreadActivate(VMMainThreadID);
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
    
    MachineFileWrite(filedescriptor, data, *length, callbackMachineFileWrite, length);
    
    while (MACHINE_FILE_WRITE_STATUS != 1) {} // FIXME Multi When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
    
    MACHINE_FILE_WRITE_STATUS = 0; // reset
    
    if (*length < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileWrite(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_WRITE_STATUS = 1;
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
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_OPEN_STATUS = 1;
}


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
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_SEEK_STATUS = 1;
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
    if ((data==NULL) || (length==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileRead(filedescriptor, data, *length, callbackMachineFileRead, length);
    
    while (MACHINE_FILE_READ_STATUS != 1) {} // FIXME Multi When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
    MACHINE_FILE_READ_STATUS = 0; // reset
    
    if (*length < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileRead(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_READ_STATUS = 1;
}

TVMStatus VMFileClose(int filedescriptor) {
    int status = -1; // 0 is success, < 0 is failure
    MachineFileClose(filedescriptor, callbackMachineFileClose, &status);

    while (MACHINE_FILE_CLOSE_STATUS != 1) {} // FIXME Multi When a thread calls VMFileClose() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful closing of the file is completed.

    MACHINE_FILE_CLOSE_STATUS = 0; // reset
    
    //if (*status < 0) {
    if (status < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileClose(void *calldata, int result) {

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_CLOSE_STATUS = 1;
}



TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {

    /*
     VMThreadCreate() creates a thread in the virtual machine.
     Once created the thread is in the dead state VM_THREAD_STATE_DEAD.
     The entry parameter specifies the function of the thread - i.e. it is a function pointer to a function that the thread is going to run?
     param specifies the parameter that is passed to the function referred to by entry
     
     The size of the threads stack is specified by memsize - how much space on the stack this thread gets? we don't decide this, the app does
     the priority is specified by prio --> a TVMThreadPriority value
     The thread identifier is PUT into the location specified by the tid parameter...as in we put it there?
     
     Return Value
     Upon successful creation of the thread VMThreadCreate() returns VM_STATUS_SUCCESS.
     */
    
    // VMThreadCreate() returns VM_STATUS_ERROR_INVALID_PARAMETER if either entry or tid is NULL.
    if ((entry==NULL) || (tid==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    TCB thread(entry, prio);
    
    
    // void MachineContextCreate(SMachineContextRef mcntxref, void (*entry)(void *), void *param, void *stackaddr, size_t stacksize);
    // #define MachineContextSwitch(mcntxold,mcntxnew)    \
    if(setjmp((mcntxold)->DJumpBuffer) == 0) longjmp((mcntxnew)->DJumpBuffer, 1)

    
    
}


/*TVMStatus VMThreadDelete(TVMThreadID thread);
TVMStatus VMThreadActivate(TVMThreadID thread);
TVMStatus VMThreadTerminate(TVMThreadID thread);
TVMStatus VMThreadID(TVMThreadIDRef threadref);
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref);
TVMStatus VMThreadSleep(TVMTick tick);

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref);
TVMStatus VMMutexDelete(TVMMutexID mutex);
TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref);
TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout);
TVMStatus VMMutexRelease(TVMMutexID mutex);
*/
    
    


}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 */

/*
 Pseudocode for Scheduling Algorithm
 
 
*/