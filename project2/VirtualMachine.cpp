#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include <vector>
#include <queue>

extern "C" {
using namespace std;

#define VM_THREAD_PRIORITY_IDLE                  ((TVMThreadPriority)0x00)

TVMThreadID CURRENT_THREAD = 0;
vector<TCB*> threadVector;
priority_queue<TCB*> readyQueue;

volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values
volatile int MACHINE_FILE_OPEN_STATUS = 0;
volatile int MACHINE_FILE_SEEK_STATUS = 0;
volatile int MACHINE_FILE_READ_STATUS = 0;
volatile int MACHINE_FILE_WRITE_STATUS = 0;
volatile int MACHINE_FILE_CLOSE_STATUS = 0;

TVMMainEntry VMLoadModule(const char *module);

void idle(void* x);

void callbackMachineRequestAlarm(void *calldata);
void callbackMachineFileOpen(void *calldata, int result);
void callbackMachineFileSeek(void *calldata, int result);
void callbackMachineFileRead(void *calldata, int result);
void callbackMachineFileWrite(void *calldata, int result);
void callbackMachineFileClose(void *calldata, int result);

bool threadExists(TVMThreadID thread);
    
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
        SMachineContextRef mcntxrefMain; // placeholder: this will be assigned when context is switched
        TVMThreadIDRef mainTID = NULL;
        TCB mainThread(mainTID, NULL, 0, VM_THREAD_STATE_RUNNING, VM_THREAD_PRIORITY_NORMAL, NULL, NULL, mcntxrefMain);
        threadVector.push_back(&mainThread);
        //TCB mainThread();
        
        //TVMThreadIDRef idleTID = NULL;
		//VMThreadCreate(idle, NULL, 0x10000, VM_THREAD_PRIORITY_IDLE, idleTID); // pushed back in VMThreadCreate

        module(argc, argv);
        
        // activate and run idle thread
        
        // switch back to main thread
        
        return VM_STATUS_SUCCESS;
    }
}

void idle(void* x)  {
	while (true) {}
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

    if ((entry==NULL) || (tid==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

	char *stackPointer = new char[memsize];
	SMachineContextRef mcntxref;
	tid = NULL;
	MachineContextCreate(mcntxref, entry, param, stackPointer, memsize);
	TCB thread(tid, stackPointer, memsize, VM_THREAD_STATE_DEAD, prio, entry, param, mcntxref);
	threadVector.push_back(&thread);
	
	return VM_STATUS_SUCCESS;
    
}

TVMStatus VMThreadID(TVMThreadIDRef threadref) {
	if (threadref == NULL) {
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else {
        TVMThreadID id = threadVector[CURRENT_THREAD]->getThreadID();
        threadref = &id;
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMThreadDelete(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setDeleted();
        return VM_STATUS_SUCCESS;
    }
}
    
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (stateref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        TVMThreadState state = threadVector[thread]->getTVMThreadState();
        stateref = &state;
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadActivate(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
        readyQueue.push(threadVector[thread]);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadTerminate(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() == VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_DEAD);
        // Thread will still be in the ready or waiting queue, so we need to check the state in Scheduler()
        // FIXME and must release any mutexes that it currently holds.
        // FIXME??? Scheduling: The termination of a thread can trigger another thread to be scheduled.
        return VM_STATUS_SUCCESS;
    }
}
    
bool threadExists(TVMThreadID thread) {
    bool exists;
    if (thread >= threadVector.size())
    {
        exists = 0;
    }
    else if (threadVector[thread]->getDeleted() == 1) {
        exists = 0;
    }
    else {
        exists = 1;
    }
    return exists;
}
    
/*
TVMStatus VMThreadSleep(TVMTick tick);

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref);
TVMStatus VMMutexDelete(TVMMutexID mutex);
TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref);
TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout);
TVMStatus VMMutexRelease(TVMMutexID mutex);
*/
    
/*void entrySkeleton(TVMThreadEntry entry, void *params) { // FIXME???
	entry(params);
	VMThreadTerminate(CURRENT_THREAD);
}*/


void Scheduler() {
	
    // Thread will still be in the ready or waiting queue, so we need to check the state in Scheduler()

    
/*
 Pseudocode for Scheduling Algorithm
 
 So you call the machine function, your current thread blocks, and you switch to the next ready thread.
 * Once this other thread gets your callback, you will put the thread that was blocking in the ready state, and then check if its priority is higher than the current running thread.  if it is, then you switch threads. if its not then it stays in the ready state.
 
*/


}
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 *
 *  // @567 @630 @676 @716
 
 //  Christopher Nitta: You may not need to use MachineContextSave or MachineContextRestore.
 // The MachineContextSwitch will probably be what you will want to use. It saves the existing context and restores the new one.
 
 */


/* To Do List
 - test main and idle threads...the very basics
 - update VMThreadSleep (and associated functions) to be multithreaded
 
 
 
 */
