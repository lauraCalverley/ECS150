#include <stdlib.h>
#include <unistd.h>

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include "Mutex.h"
#include <vector>
#include <queue>
#include <iostream>

extern "C" {
using namespace std;

//global variables
#define VM_THREAD_PRIORITY_IDLE                  ((TVMThreadPriority)0x00)
TVMThreadID CURRENT_THREAD = 0;
vector<TCB*> threadVector;
vector<Mutex*> mutexVector;
priority_queue<TCB> readyQueue;
int TICKMS;
volatile int TICK_COUNT = 0;

//function prototypes
bool mutexExists(TVMMutexID id);
TVMMainEntry VMLoadModule(const char *module);
void VMUnloadModule(void);
void idle(void* x);
void callbackMachineRequestAlarm(void *calldata);
void callbackMachineFile(void* threadID, int result);
bool threadExists(TVMThreadID thread);
void entrySkeleton(void *thread);
void Scheduler(int transition, TVMThreadID thread);



TVMStatus VMStart(int tickms, int argc, char *argv[]) {
    TICKMS = tickms;
    MachineInitialize();
    MachineRequestAlarm(tickms*1000, callbackMachineRequestAlarm, NULL); 
    TVMMainEntry module = VMLoadModule(argv[0]);
    if (module == NULL) {
        return VM_STATUS_FAILURE;
    }
    else {
        // create main TCB
        SMachineContext mcntxMain; 
        TVMThreadID mainTID = threadVector.size();
        TCB* mainThread = new TCB(mainTID, NULL, 0, VM_THREAD_STATE_RUNNING, VM_THREAD_PRIORITY_NORMAL, NULL, NULL, mcntxMain);
        threadVector.push_back(mainThread);

        // create idle thread and TCB; activate idle thread
        TVMThreadID idleTID;
        VMThreadCreate(idle, NULL, 0x10000, VM_THREAD_PRIORITY_IDLE, &idleTID);
        VMThreadActivate(idleTID);
        
        MachineEnableSignals();
        module(argc, argv);

        VMUnloadModule();

        //deallocate memory
        for(int i = 0; i < threadVector.size(); i++){
            delete[] threadVector[i]->getStackPointer();
            delete threadVector[i];
        }
        for(int i = 0; i < mutexVector.size(); i++){
            delete mutexVector[i];
        }
        
        return VM_STATUS_SUCCESS;
    }
}

void idle(void* x)  {
    while (true) {}
}

void entrySkeleton(void *thread) {
    TCB* theThread = (TCB*)thread;
    TVMThreadEntry entry = theThread->getTVMThreadEntry();
    void* entryParams = theThread->getParams();
    MachineEnableSignals();
    entry(entryParams);
    VMThreadTerminate(CURRENT_THREAD);
}



//Callback functions
void callbackMachineRequestAlarm(void *calldata) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    TICK_COUNT++;
    
    for (int i=0; i < threadVector.size(); i++) {
        if ((threadVector[i]->getDeleted() == 0) && (threadVector[i]->getSleepCount() > 0)) {
            threadVector[i]->decrementSleepCount();
            
            if (threadVector[i]->getSleepCount() == 0) {
                Scheduler (1, i);
            }
        }
    }

    for (int i=0; i < threadVector.size(); i++) {
        if ((threadVector[i]->getDeleted() == 0) && (threadVector[i]->getMutexWaitCount() > 0)) {
            threadVector[i]->decrementMutexWaitCount();
            
            if (threadVector[i]->getMutexWaitCount() == 0) {
                Scheduler (1, i);
            }
        }
    }    
    
    Scheduler(3, CURRENT_THREAD);
    MachineResumeSignals(&sigState);
}

void callbackMachineFile(void* threadID, int result) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    threadVector[*(int*)threadID]->setMachineFileFunctionResult(result);
    Scheduler(1, threadVector[*(int*)threadID]->getThreadID());
    
    MachineResumeSignals(&sigState);
}



//VM Tick functions
TVMStatus VMTickMS(int *tickmsref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    if (tickmsref == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickmsref = TICKMS;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMTickCount(TVMTickRef tickref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    if (tickref == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickref = TICK_COUNT;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}


//VM File functions
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((data==NULL) || (length==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileWrite(filedescriptor, data, *length, callbackMachineFile, &savedCURRENTTHREAD);
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

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((filename==NULL) || (filedescriptor==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileOpen(filename, flags, mode, callbackMachineFile, &savedCURRENTTHREAD);
    Scheduler(6,CURRENT_THREAD);

    *filedescriptor = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
    
    if (*filedescriptor < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileSeek(filedescriptor, offset, whence, callbackMachineFile, &savedCURRENTTHREAD);
    Scheduler(6,CURRENT_THREAD);

    *newoffset = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();

    if (*newoffset < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((data==NULL) || (length==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileRead(filedescriptor, data, *length, callbackMachineFile, &savedCURRENTTHREAD);
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

TVMStatus VMFileClose(int filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    MachineFileClose(filedescriptor, callbackMachineFile, &savedCURRENTTHREAD);
    Scheduler(6,CURRENT_THREAD);

    int status = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
    
    if (status < 0) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}



//VM Thread functions
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

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((entry==NULL) || (tid==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    char *stackPointer = new char[memsize];
	SMachineContext mcntx;
    
    TVMThreadID newThreadID = threadVector.size();
    TCB* thread = new TCB(newThreadID, stackPointer, memsize, VM_THREAD_STATE_DEAD, prio, entry, param, mcntx);
    threadVector.push_back(thread);
    *tid = threadVector[newThreadID]->getThreadID();
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



//VM Mutex functions    
bool mutexExists(TVMMutexID id) {
    bool exists;
    if (id >= mutexVector.size())
    {
        exists = 0;
    }
    else if (mutexVector[id]->deleted == 1) {
        exists = 0;
    }
    else {
        exists = 1;
    }
    return exists;
}

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if (mutexref == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    TVMMutexID id = mutexVector.size();
    Mutex* mymutex = new Mutex(id);
    mymutex.owner = threadVector[CURRENT_THREAD]->getThreadID();//test
    mymutex.value = 0; //test
    mutexVector.push_back(mymutex);
    *mutexref = mutexVector[id]->id;
    
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
    
}

TVMStatus VMMutexDelete(TVMMutexID mutex) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (!mutexExists(mutex)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    if (mutexVector[mutex]->value == 1) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        mutexVector[mutex]->deleted = 1;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    
    
    MachineResumeSignals(&sigState);
}

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    cout << "value: " << mutexVector[*ownerref]->value << endl;

    if (!mutexExists(mutex)) {
        MachineResumeSignals(&sigState);
        cout << "mutex: " << mutex << " doesn't exist" << endl;
        return VM_STATUS_ERROR_INVALID_ID;
    }
    if (ownerref == NULL) {
        cout << "ownerref is null" << endl;
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    if (mutexVector[mutex]->value == 1) { // mutex is unlocked
        cout << "mutex is unlocked" << endl;
        MachineResumeSignals(&sigState);
        return VM_THREAD_ID_INVALID;
    }
    else {
        *ownerref = mutexVector[mutex]->owner;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (!mutexExists(mutex)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }
    
    if (timeout == VM_TIMEOUT_IMMEDIATE) {
        if (mutexVector[mutex]->value == 0) {
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        else {
            // gets the mutex
            mutexVector[mutex]->owner = threadVector[CURRENT_THREAD]->getThreadID();
            mutexVector[mutex]->value = 0;
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }
    else if (timeout == VM_TIMEOUT_INFINITE) {
        if (mutexVector[mutex]->value == 1) {
            mutexVector[mutex]->owner = threadVector[CURRENT_THREAD]->getThreadID();
            mutexVector[mutex]->value = 0;
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
        else {
            mutexVector[mutex]->waiting.push(*threadVector[CURRENT_THREAD]);
            Scheduler(6, CURRENT_THREAD);
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }
    else {
        if (mutexVector[mutex]->value == 1) {
            mutexVector[mutex]->owner = threadVector[CURRENT_THREAD]->getThreadID();
            mutexVector[mutex]->value = 0;
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
        else {
            mutexVector[mutex]->waiting.push(*threadVector[CURRENT_THREAD]);
            threadVector[CURRENT_THREAD]->setMutexWaitCount(timeout);
            Scheduler(6, CURRENT_THREAD);
            
            if (mutexVector[mutex]->owner == CURRENT_THREAD) {
                MachineResumeSignals(&sigState);
                return VM_STATUS_SUCCESS;
            }
            else {
                MachineResumeSignals(&sigState);
                return VM_STATUS_FAILURE;
            }
        }
    }
}

TVMStatus VMMutexRelease(TVMMutexID mutex) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (!mutexExists(mutex)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }

    if (mutexVector[mutex]->owner != threadVector[CURRENT_THREAD]->getThreadID()) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    
    if (!mutexVector[mutex]->waiting.empty()) {
        mutexVector[mutex]->owner = mutexVector[mutex]->waiting.top().getThreadID();
        mutexVector[mutex]->waiting.pop();
        mutexVector[mutex]->value = 0;
        Scheduler(1, mutexVector[mutex]->owner);
    }
    else {
        mutexVector[mutex]->value = 1;
    }
    
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
}



//Scheduler
void Scheduler(int transition, TVMThreadID thread) {
    
    switch (transition) {
        case 1: { // I/O, acquire mutex or timeout
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[thread]);

            if (threadVector[thread]->getTVMThreadPriority() > threadVector[CURRENT_THREAD]->getTVMThreadPriority()) {
                Scheduler(3, thread);
            }
            break;
        }
        case 2: { // Scheduler selects process // Scheduler (2, CURRENT_THREAD)
            TVMThreadID readyThread = readyQueue.top().getThreadID();
            threadVector[readyThread]->setTVMThreadState(VM_THREAD_STATE_RUNNING);
            readyQueue.pop();
            CURRENT_THREAD = readyThread;
            MachineContextSwitch(threadVector[thread]->getMachineContext(),threadVector[CURRENT_THREAD]->getMachineContext());
            break;
        }
        case 3: { // Process quantum up // only ever called by Scheduler(), in alarm callback, call Scheduler(2, NULL)
            threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[CURRENT_THREAD]);
            Scheduler(2, CURRENT_THREAD); // sort of loses track of thread that should be moved from ready to running, but easy to get with top() in case 2
            break;
        }
        case 4: { // Process terminates
            TVMThreadState oldThreadState = threadVector[thread]->getTVMThreadState();
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_DEAD);

            for (int i=0; i < mutexVector.size(); i++) {
                // check owner and waiters for each mutex to see if thread being terminated is owner or a waiter, if so: remove
                if (mutexExists(i)) {
                    if (thread == mutexVector[i]->owner) {
                        if (!mutexVector[i]->waiting.empty()) {
                            mutexVector[i]->owner = mutexVector[i]->waiting.top().getThreadID();
                            mutexVector[i]->waiting.pop();
                            mutexVector[i]->value = 0;
                            Scheduler(1, mutexVector[i]->owner);
                        }
                        else {
                            mutexVector[i]->value = 1;
                        }
                    }
                    else {
                        vector<TCB> tempMutexWaitingVector;
                        
                        // remove thread from mutex.waiting priority queue
                        while (!mutexVector[i]->waiting.empty()) {
                            TCB top = mutexVector[i]->waiting.top();
                            if (top.getThreadID() != threadVector[thread]->getThreadID()) {
                                tempMutexWaitingVector.push_back(top);
                            }
                            mutexVector[i]->waiting.pop();
                        }
                        for (int i=0; i < tempMutexWaitingVector.size(); i++) {
                            mutexVector[i]->waiting.push(tempMutexWaitingVector[i]);
                        }
                    }
                }
            }
            
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
                    // nothing
                    break;
                }
            }
            break;
        }
        case 5: { // Process activated
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(*threadVector[thread]);
            
            if (threadVector[thread]->getTVMThreadPriority() > threadVector[CURRENT_THREAD]->getTVMThreadPriority()) {
                Scheduler(3, thread);
            }

            break;
        }
        case 6: { // Process blocks // Scheduler(6. CURRENT_THREAD)
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_WAITING);
            Scheduler(2, thread);
            break;
        }
    }
}    
}   

