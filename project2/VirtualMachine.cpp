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
priority_queue<TCB> readyQueue;

int TICKMS;
volatile int TICK_COUNT = 0;
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
void callbackMachineFileWrite(void* threadID, int result);
void callbackMachineFileClose(void *calldata, int result);

bool threadExists(TVMThreadID thread);
void entrySkeleton(void *thread);
void Scheduler(int transition, TVMThreadID thread);


TVMStatus VMTickMS(int *tickmsref) {
    if (tickmsref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickmsref = TICKMS;
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMTickCount(TVMTickRef tickref) {
    if (tickref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickref = TICK_COUNT;
        return VM_STATUS_SUCCESS;
    }
}
    
    
TVMStatus VMStart(int tickms, int argc, char *argv[]) {
    TICKMS = tickms;
    MachineInitialize();
    MachineRequestAlarm(tickms*1000, callbackMachineRequestAlarm, NULL); // 2nd arg is a function pointer
    TVMMainEntry module = VMLoadModule(argv[0]);
    if (module == NULL) {
        return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
    }
    else {
        // create main TCB
        SMachineContext mcntxMain; // placeholder: this will be assigned when context is switched
        TVMThreadID mainTID = threadVector.size();
        TCB* mainThread = new TCB(mainTID, NULL, 0, VM_THREAD_STATE_RUNNING, VM_THREAD_PRIORITY_NORMAL, NULL, NULL, mcntxMain);
        threadVector.push_back(mainThread);

        // create idle thread and TCB; activate idle thread
        TVMThreadID idleTID;
		VMThreadCreate(idle, NULL, 0x10000, VM_THREAD_PRIORITY_IDLE, &idleTID); // pushed back in VMThreadCreate
        VMThreadActivate(idleTID);
        
        MachineEnableSignals();
        module(argc, argv);
        
        // FIXME deallocate memory for TCBs and such
        
        return VM_STATUS_SUCCESS;
    }
}

void idle(void* x)  {
	while (true) {}
}

void callbackMachineRequestAlarm(void *calldata) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    TICK_COUNT++;
    
    for (int i=0; i < threadVector.size(); i++) { // IMPROVEMENT sleepVector idea
        if ((threadVector[i]->getDeleted() == 0) && (threadVector[i]->getSleepCount() > 0)) {
            threadVector[i]->decrementSleepCount();
            
            if (threadVector[i]->getSleepCount() == 0) {
                Scheduler (1, i);
            }
        }
    }
    Scheduler(3, CURRENT_THREAD);
    MachineResumeSignals(&sigState);
}

    
TVMStatus VMThreadSleep(TVMTick tick) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if (tick == VM_TIMEOUT_INFINITE) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else if (tick == VM_TIMEOUT_IMMEDIATE) {
        Scheduler(3, CURRENT_THREAD);
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    else {
        threadVector[CURRENT_THREAD]->setSleepCount(tick);
        Scheduler(6, CURRENT_THREAD);

        while (threadVector[CURRENT_THREAD]->getSleepCount() != 0) { // for debugging
            cout << "this shouldn't ever print";
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((data==NULL) || (length==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileWrite(filedescriptor, data, *length, callbackMachineFileWrite, &savedCURRENTTHREAD);
    Scheduler(6,CURRENT_THREAD);
    
    *length = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
    
    if (*length < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileWrite(void* threadID, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    threadVector[*(int*)threadID]->setMachineFileFunctionResult(result);
    Scheduler(1, threadVector[*(int*)threadID]->getThreadID());

    MachineResumeSignals(&sigState);
}


TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((filename==NULL) || (filedescriptor==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileOpen(filename, flags, mode, callbackMachineFileOpen, filedescriptor);
    // save the CURRENT_THREAD id
    // while (threadVector[savedCURRENTTHREAD]->getState()==WAITING)
    while (MACHINE_FILE_OPEN_STATUS != 1) {} // FIXME Multi When a thread calls VMFileOpen() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful opening of the file is completed.
    MACHINE_FILE_OPEN_STATUS = 0; // reset

    if (*filedescriptor < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileOpen(void *calldata, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_OPEN_STATUS = 1;
    MachineResumeSignals(&sigState);
}


TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    MachineFileSeek(filedescriptor, offset, whence, callbackMachineFileSeek, newoffset);
    
    while (MACHINE_FILE_SEEK_STATUS != 1) {} // FIXME Multi When a thread calls VMFileSeek() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful seeking in the file is completed.
    MACHINE_FILE_SEEK_STATUS = 0; // reset
    
    if (*newoffset < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileSeek(void *calldata, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_SEEK_STATUS = 1;
    MachineResumeSignals(&sigState);
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((data==NULL) || (length==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileRead(filedescriptor, data, *length, callbackMachineFileRead, length);
    
    while (MACHINE_FILE_READ_STATUS != 1) {} // FIXME Multi When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
    MACHINE_FILE_READ_STATUS = 0; // reset
    
    if (*length < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileRead(void *calldata, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_READ_STATUS = 1;
    MachineResumeSignals(&sigState);
}

TVMStatus VMFileClose(int filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    int status = -1; // 0 is success, < 0 is failure
    MachineFileClose(filedescriptor, callbackMachineFileClose, &status);

    while (MACHINE_FILE_CLOSE_STATUS != 1) {} // FIXME Multi When a thread calls VMFileClose() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful closing of the file is completed.

    MACHINE_FILE_CLOSE_STATUS = 0; // reset
    
    //if (*status < 0) {
    if (status < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileClose(void *calldata, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_CLOSE_STATUS = 1;
    MachineResumeSignals(&sigState);
}

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    /*if ((entry==NULL) || (tid==NULL)) {
        cout << "";
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }*/
    if (entry==NULL) {
        cout << ""; //FIMXE cout issue - w/o, it seg faults // NITTA says there is a bigger issue with our stack and such
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    if (tid==NULL) {
        cout << ""; //FIMXE cout issuecle - w/o, it seg faults
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

	char *stackPointer = new char[memsize];
	SMachineContext mcntx;
    
    TVMThreadID newThreadID = threadVector.size();
    TCB* thread = new TCB(newThreadID, stackPointer, memsize, VM_THREAD_STATE_DEAD, prio, entry, param, mcntx);
    threadVector.push_back(thread);
    *tid = threadVector[newThreadID]->getThreadID(); // (OPTIONAL) FIXME: if id was later updated, don't think variable external to this function would be made aware of the change...
    MachineResumeSignals(&sigState);
	return VM_STATUS_SUCCESS;
    
}

TVMStatus VMThreadID(TVMThreadIDRef threadref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
	if (threadref == NULL) {
        MachineResumeSignals(&sigState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else {
        *threadref = threadVector[CURRENT_THREAD]->getThreadID();
        MachineResumeSignals(&sigState);
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMThreadDelete(TVMThreadID thread) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (!threadExists(thread)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setDeleted();
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}
    
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if (!threadExists(thread)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (stateref == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *stateref = threadVector[thread]->getTVMThreadState();
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadActivate(TVMThreadID thread) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    if (!threadExists(thread)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        MachineContextCreate(threadVector[thread]->getMachineContext(), entrySkeleton, threadVector[thread], threadVector[thread]->getStackPointer(), threadVector[thread]->getStackSize());
        Scheduler(5, thread);
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadTerminate(TVMThreadID thread) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    if (!threadExists(thread)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() == VM_THREAD_STATE_DEAD) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        Scheduler(4, thread);
        MachineResumeSignals(&sigState);
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
TVMStatus VMMutexCreate(TVMMutexIDRef mutexref);
TVMStatus VMMutexDelete(TVMMutexID mutex);
TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref);
TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout);
TVMStatus VMMutexRelease(TVMMutexID mutex);
*/
    
//void entrySkeleton(TVMThreadEntry entry, void *params) { // FIXME???
void entrySkeleton(void *thread) { // FIXME???
    TCB* theThread = (TCB*)thread;
    TVMThreadEntry entry = theThread->getTVMThreadEntry();
    void* entryParams = theThread->getParams();
    MachineEnableSignals(); // temp // FIXME???
    entry(entryParams);
	VMThreadTerminate(CURRENT_THREAD);
}


void Scheduler(int transition, TVMThreadID thread) {
    
    switch (transition) {
        case 1: { // I/O, acquire mutex or timeout
            // assume callbacks, etc. receive the TCB/TCB* of the thread that called the function associated with the callback
            // pop mutexQueue          // have a sleepVector, mutexQueue
            //cout << "IN CASE 1" << endl;
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[thread]);

            if (threadVector[thread]->getTVMThreadPriority() > threadVector[CURRENT_THREAD]->getTVMThreadPriority()) {
                //cout << "new thread is high priority than current thread" << endl;
                Scheduler(3, thread);
            }
            break;
        }
        case 2: { // Scheduler selects process // Scheduler (2, CURRENT_THREAD)
            //cout << "IN CASE 2" << endl;
            TVMThreadID readyThread = readyQueue.top().getThreadID();
            threadVector[readyThread]->setTVMThreadState(VM_THREAD_STATE_RUNNING);
            readyQueue.pop();
            CURRENT_THREAD = readyThread;
            MachineContextSwitch(threadVector[thread]->getMachineContext(),threadVector[CURRENT_THREAD]->getMachineContext());
            break;
        }
        case 3: { // Process quantum up // only ever called by Scheduler(), in alarm callback, call Scheduler(2, NULL)
            //cout << "IN CASE 3" << endl;
            // thread parameter is the thread that is about to start running
            //  Each thread gets one quantum of time (one tick). They are then put in the ready state and scheduling is done, this means that the scheduler will need to be called in the alarm callback.
            threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[CURRENT_THREAD]);
            Scheduler(2, CURRENT_THREAD); // sort of loses track of thread that should be moved from ready to running, but easy to get with top() in case 2
            break;
        }
        case 4: { // Process terminates
            //cout << "IN CASE 4" << endl;
            TVMThreadState oldThreadState = threadVector[thread]->getTVMThreadState();
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_DEAD);

            switch (oldThreadState) {
                case VM_THREAD_STATE_RUNNING: {
                    Scheduler(2, thread);
                    break;
                }
                case VM_THREAD_STATE_READY: {
                    vector<TCB> tempReadyVector;
                    
                    // remove thread from readyQueue
                    while (!readyQueue.empty()) {
                        TCB top = readyQueue.top();
                        if (top.getThreadID() != threadVector[thread]->getThreadID()) {
                            tempReadyVector.push_back(top);
                        }
                        readyQueue.pop();
                    }
                    for (int i=0; i < tempReadyVector.size(); i++) {
                        readyQueue.push(tempReadyVector[i]);
                    }
                    break;
                }
                case VM_THREAD_STATE_WAITING: {
                    // deal mutext waiting queue
                    // deal with sleep vector if we make one
                    break;
                }
            }
            break;
        }
        case 5: { // Process activated
            //cout << "IN CASE 5" << endl;
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[thread]);
            
            if (threadVector[thread]->getTVMThreadPriority() > threadVector[CURRENT_THREAD]->getTVMThreadPriority()) {
                Scheduler(3, thread);
            }

            break;
        }
        case 6: { // Process blocks // Scheduler(6. CURRENT_THREAD)
            //cout << "IN CASE 6" << endl;
            // move the next 2 lines to the functions that cause something to go to waiting (mutex, file functions, VMThreadSleep)
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_WAITING);
            Scheduler(2, thread);
            break;
        }
    }
}
    
    // MCS occurs when you swap the Running threads
    
    // 3 4 and 6 all call 2
    
    // in VMThreadTerminate
    // Thread will still be in the ready or waiting queue, so we need to check the state in Scheduler()

    // 6 is waiting to acquire mutex successfully, waiting for I/O (i.e. a callback from one of the VMFile functions, or sleeping
    // 1 is acquired mutex, got a/in a callback saying I/O was done, or no longer sleeping (SLEEPCOUNT for the thread = 0)
    
    // VMFile callback functions, VMThreadSleep, Mutex function callbacks(presumably) will call Scheduler(1);
    // VMFile functions will call Scheduler(6); then Scheduler(2)
    // VMThreadTerminate will call Scheduler(4); if terminated thread was RUNNING, call Scheduler(2);
    //* VMThreadActivate will call Scheduler (5)
    // MachineAlarmCallback will call Scheduler(3) and then Scheduler(2)
    
    
    // Scheduler() will figure out what the next thread/context is (readyQueue top)
    //change oldState
    //change newState
    //MCS()
    
/*
 Pseudocode for Scheduling Algorithm
 
 So you call the machine function, your current thread blocks, and you switch to the next ready thread.
 * Once this other thread gets your callback, you will put the thread that was blocking in the ready state, and then check if its priority is higher than the current running thread.  if it is, then you switch threads. if its not then it stays in the ready state.
 
*/
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 *
 *  // @567 @630 @676 @716
 */


/* To Do List
 Scheduler()
 redo all VMFile* - call Scheduler(transition, threadthatisnolongerwaiting), need to pass the thread that is being worked on in the callback data, need to add a "waiting for callback" data member to the TCB class
 -- combine all the File callbacks into one
 -- in callback, save CURRENT_THREAD (because WILL change elsewhere while waiting)
 -- while (savedCurrentThread == WAITING)
 
 -- in callback, set the thread it's passed (in callback data) to READY instead of WAITING
 
 // suspend when you enter VMFunction
 // resume before you leave VMFunction
 
 // signal stuff in all the functions, including callbacks
 enable signals right before you go into the entry
 
 VMMutexCreate
 VMMutexDelete
 VMMutexQuery
 VMMutexAcquire
 VMMutexRelease
 */
