#include <stdlib.h>
#include <unistd.h>

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include "Mutex.h"
#include "MemoryPool.h"
#include <cstring>
#include <vector>
#include <queue>

#include <iostream> // temp

extern "C" {
using namespace std;

//global variables
#define VM_THREAD_PRIORITY_IDLE                  ((TVMThreadPriority)0x00)
TVMThreadID CURRENT_THREAD = 0;
vector<TCB*> threadVector;
vector<Mutex*> mutexVector;
priority_queue<TCB> readyQueue;
vector<MemoryPool*> memoryPoolVector;
priority_queue<TCB> memoryPoolWaitQueue;
int TICKMS;
volatile int TICK_COUNT = 0;
char *BASE_ADDRESS = NULL;
TVMMemorySize SHARED_MEMORY_SIZE = 0;
char *HEAP_BASE = NULL;
TVMMemorySize HEAP_BASE_SIZE = 0;
const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0; // FIXME - redeclaration of VM_MEMORY_POOL_ID_SYSTEM???
const TVMMemoryPoolID VM_MEMORY_POOL_ID_SHARED_MEMORY = 1;


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

TVMStatus VMStart(int tickms, TVMMemorySize heapsize, TVMMemorySize sharedsize, int argc, char *argv[]) {
    TICKMS = tickms;

    HEAP_BASE_SIZE = heapsize;
    HEAP_BASE = new char[HEAP_BASE_SIZE];
    
    TVMMemoryPoolID systemPoolID = 0; // FIXME - bad, use VM_MEMORY_POOL_ID_SYSTEM!!!
    VMMemoryPoolCreate(HEAP_BASE, HEAP_BASE_SIZE, &systemPoolID);
    //const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = systemPoolID; // FIXME - redeclaration of VM_MEMORY_POOL_ID_SYSTEM???
    
    SHARED_MEMORY_SIZE = sharedsize;
    BASE_ADDRESS = (char*)MachineInitialize(SHARED_MEMORY_SIZE); // FIXME - char* ??? // FIXME - check for NULL? then what?
    TVMMemoryPoolID sharedMemoryPoolID = 1; // FIXME - bad, use VM_MEMORY_POOL_ID_SYSTEM!!!
    VMMemoryPoolCreate(BASE_ADDRESS, SHARED_MEMORY_SIZE, &sharedMemoryPoolID);

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
        VMThreadCreate(idle, NULL, 0x100000, VM_THREAD_PRIORITY_IDLE, &idleTID);
        VMThreadActivate(idleTID);
        
        MachineEnableSignals();
        module(argc, argv);

        VMUnloadModule();

        //deallocate memory
        for(int i = 0; i < threadVector.size(); i++){
            delete threadVector[i];
        }
        for(int i = 0; i < mutexVector.size(); i++){
            delete mutexVector[i];
        }
        for(int i = 0; i < memoryPoolVector.size(); i++){
            delete memoryPoolVector[i];
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


//MemoryPool functions
bool memoryPoolExists(TVMMemoryPoolID memPoolID) {
    bool exists;
    if (memPoolID >= memoryPoolVector.size())
    {
        exists = 0;
    }
    else if (memoryPoolVector[memPoolID]->getDeleted() == 1) {
        exists = 0;
    }
    else {
        exists = 1;
    }
    return exists;
}
    
    
TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((base == NULL) || (memory == NULL) || (size == 0)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    TVMMemoryPoolID memoryID = memoryPoolVector.size();
    
    MemoryPool* memPool = new MemoryPool(base, size, &memoryID);
    memoryPoolVector.push_back(memPool);
    *memory = memoryPoolVector[memoryID]->getMemoryPoolID();

    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
}
    
TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((!memoryPoolExists(memory)) || (bytesleft == NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    *bytesleft = memoryPoolVector[memory]->bytesLeft();
    
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
}

TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((!memoryPoolExists(memory)) || (size == 0) || (pointer == NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    TVMMemorySize roundedSize;
    if ((size % 64) != 0) {
        roundedSize = ((size / 64) + 1) * 64;
    }
    else {
        roundedSize = size;
    }
    
    char* memoryLocation = memoryPoolVector[memory]->allocate(roundedSize);
    if (memoryLocation == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
    }
    else {
        *pointer = memoryLocation; // FIXME??
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((!memoryPoolExists(memory)) || (pointer == NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    char *deallocatedLocation = memoryPoolVector[memory]->deallocate((char*)pointer);
    
    if (deallocatedLocation == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}


TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if (!memoryPoolExists(memory)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    if (memoryPoolVector[memory]->getAllocatedListSize() == 0) {
        memoryPoolVector[memory]->setDeleted();
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_STATE;
    }

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
    
    while (!memoryPoolWaitQueue.empty()) {
        cout << "in callback while loop and memoryPoolWaitQueue is not empty" << endl;
        void *sharedMemory;
        TVMThreadID topThreadID = memoryPoolWaitQueue.top().getThreadID();
        if (threadVector[topThreadID]->getDeleted() == 0) {
            VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sharedMemory);
        }
        else {
            memoryPoolWaitQueue.pop();
            continue;
        }
        if (sharedMemory == NULL) {
            break;
        }
        else {
            threadVector[topThreadID]->setSharedMemoryPointer(sharedMemory);
            memoryPoolWaitQueue.pop();
            Scheduler(1, topThreadID);
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

    
    
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((data==NULL) || (length==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
    
    void *sharedMemory;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sharedMemory);
    
    if(sharedMemory == NULL){
        cout << "no space available in fileWrite" << endl;
        memoryPoolWaitQueue.push(*threadVector[CURRENT_THREAD]);
        Scheduler(6,CURRENT_THREAD);
        sharedMemory = threadVector[CURRENT_THREAD]->getSharedMemoryPointer();
    }
    
    strncpy((char*)sharedMemory, (const char *)data, *length);
    int writeLength;
    int cumLength = 0;
    
    while (*length != 0) {
        cout << "in while loop" << endl;
        if(*length > 512) {
            writeLength = 512;
        }
        else {
            cout << "in first else" << endl;
            writeLength = *length;
        }
        MachineFileWrite(filedescriptor, (char*)sharedMemory, writeLength, callbackMachineFile, &savedCURRENTTHREAD);
        Scheduler(6,CURRENT_THREAD);

        if (threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult() < 0) {
            cout << "in second if " << endl;
            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sharedMemory);
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        cumLength += threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();

        *length -= writeLength;
        sharedMemory = (char*)sharedMemory + writeLength;
    }

    *length = cumLength;
    
    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sharedMemory);
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
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
    
    void *sharedMemory;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sharedMemory);
    
    if(sharedMemory == NULL){
        memoryPoolWaitQueue.push(*threadVector[CURRENT_THREAD]);
        Scheduler(6,CURRENT_THREAD);
        sharedMemory = threadVector[CURRENT_THREAD]->getSharedMemoryPointer();
    }
    
    int readLength;
    int cumLength = 0;
    
    while (*length != 0) {
        if(*length > 512) {
            readLength = 512;
        }
        else {
            readLength = *length;
        }
        MachineFileRead(filedescriptor, (char*)sharedMemory, readLength, callbackMachineFile, &savedCURRENTTHREAD);
        Scheduler(6,CURRENT_THREAD);
        
        int resultLength = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
        
        if (resultLength < 0) {
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        strncpy((char*)data, (char*)sharedMemory, resultLength);
        cumLength += resultLength;
        *length -= readLength;
        sharedMemory = (char*)sharedMemory + readLength;
        data = (char*)data + readLength;
    }
    
    *length = cumLength;
    
    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sharedMemory);
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;
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

    void *stackPointer;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, memsize, &stackPointer);
    
    if (stackPointer == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
    }
    else {
        SMachineContext mcntx;
        
        TVMThreadID newThreadID = threadVector.size();
        TCB* thread = new TCB(newThreadID, (char*)stackPointer, memsize, VM_THREAD_STATE_DEAD, prio, entry, param, mcntx);
        threadVector.push_back(thread);
        *tid = threadVector[newThreadID]->getThreadID();
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
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
    if (mutexVector[mutex]->value != 1) {
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

    if (!mutexExists(mutex)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_ID;
    }

    if (ownerref == NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    if (mutexVector[mutex]->value == 1) { // mutex is unlocked
        *ownerref = VM_THREAD_ID_INVALID;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
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

