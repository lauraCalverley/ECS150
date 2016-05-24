#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include "Mutex.h"
#include "MemoryPool.h"
#include "BPB.h"
#include <cstring>
#include <vector>
#include <queue>
#include <stdint.h>

#include <cstdio> //temp
#include <iostream> //temp

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
const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;
const TVMMemoryPoolID VM_MEMORY_POOL_ID_SHARED_MEMORY = 1;
BPB *theBPB;
vector<uint16_t> FAT;

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
void readSector(int fd, char *sectorData, int sectorNumber);
void storeBPB(int fd);
void storeFAT(int fd);

    
TVMStatus VMStart(int tickms, TVMMemorySize heapsize, TVMMemorySize sharedsize, const char *mount, int argc, char *argv[]) {
    TICKMS = tickms;

    HEAP_BASE_SIZE = heapsize;
    HEAP_BASE = new char[HEAP_BASE_SIZE];
    
    TVMMemoryPoolID systemPoolID = 0;
    VMMemoryPoolCreate(HEAP_BASE, HEAP_BASE_SIZE, &systemPoolID);
    
    SHARED_MEMORY_SIZE = sharedsize;
    BASE_ADDRESS = (char*)MachineInitialize(SHARED_MEMORY_SIZE);
    TVMMemoryPoolID sharedMemoryPoolID = 1;
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
        
        //load FAT
        
        TVMThreadID savedCurrentThread = CURRENT_THREAD;
        
        MachineFileOpen(mount, O_RDWR, 0600, callbackMachineFile, &savedCurrentThread);
        Scheduler(6,savedCurrentThread);
        int fd = threadVector[savedCurrentThread]->getMachineFileFunctionResult();
        
        if (fd < 0) {
            return VM_STATUS_FAILURE;
        }
        
        //
     
        
        
        storeBPB(fd);
        storeFAT(fd);
 
//        cout << "FirstRootSector" << theBPB->FirstRootSector << endl;
//        cout << "RootDirectorySectors" << theBPB->RootDirectorySectors << endl;
//        cout << "FirstDataSector" << theBPB->FirstDataSector << endl;
//        cout << "ClusterCount" << theBPB->ClusterCount << endl;
        
/*
 FAT has 4098 entries
 00000000: FFF8 FFFF END  0004 0005 0006 0007 0008
 00000010: 0009 000A 000B 000C 000D 000E 000F 0010
 00000020: 0011 0012 0013 0014 0015 0016 0017 0018
 00000030: 0019 001A 001B 001C END  001E END  0020
 00000040: 0021 END  0023 END  0025 0026 0027 0028
 00000050: 0029 002A END  002C 002D 002E 002F 0030
 00000060: 0031 0032 0033 0034 0035 0036 END  0038
 00000070: 0039 003A 003B 003C 003D 003E 003F 0040
 00000080: 0041 0042 END  0044 0045 0046 0047 0048
 00000090: 0049 004A 004B 004C 004D 004E 004F 0050
 000000A0: 0051 0052 0053 0054 END  0056 0057 0058
 000000B0: 0059 005A 005B 005C END  005E END  0060
 000000C0: END  END  0063 0064 END  0066 END  0068
 000000D0: END  006A 006B 006C 006D END  END  0070
 000000E0: END  FREE FREE FREE FREE FREE FREE FREE
 */
        
        
        
        
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


void readSector(int fd, char *sectorData, int sectorNumber) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    TVMThreadID savedCurrentThread = CURRENT_THREAD;
    
    MachineFileSeek(fd, sectorNumber*512, 0, callbackMachineFile, &savedCurrentThread);
    Scheduler(6,savedCurrentThread);
    
    MachineFileRead(fd, sectorData, 512, callbackMachineFile, &savedCurrentThread);
    Scheduler(6,savedCurrentThread);
    
//    int result = threadVector[savedCurrentThread]->getMachineFileFunctionResult();
    
//    if (result < 0) {
//        cout << "ERROR in readSector" << endl;
//    }
//    else {
//        cout << "bytes read: " << result << endl;
//    }
    MachineResumeSignals(&sigState);
}

void storeBPB(int fd) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    void *sectorData;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sectorData);
    readSector(fd, (char*)sectorData, 0);
    
    uint8_t BPB_SecPerClus = *(uint8_t *)((char*)sectorData + 13); // CITE Nitta FIXME - remove (int) cast?
//    cout << "BPB_SecPerClus " << (int)BPB_SecPerClus << endl;
    uint16_t BPB_RsvdSecCnt = *(uint16_t *)((char*)sectorData + 14); // CITE Nitta
//    cout << "BPB_RsvdSecCnt " << BPB_RsvdSecCnt << endl;
    uint8_t BPB_NumFATs = *(uint8_t *)((char*)sectorData + 16); // CITE Nitta FIXME - remove (int) cast?
//    cout << "BPB_NumFATs " << (int)BPB_NumFATs << endl;
    uint16_t BPB_RootEntCnt = *(uint16_t *)((char*)sectorData + 17); // CITE Nitta
//    cout << "BPB_RootEntCnt " << BPB_RootEntCnt << endl;
    uint16_t BPB_FATSz16 = *(uint16_t *)((char*)sectorData + 22); // CITE Nitta
//    cout << "BPB_FATSz16 " << BPB_FATSz16 << endl;
    uint32_t BPB_TotSec32 = *(uint32_t *)((char*)sectorData + 32); // CITE Nitta
//    cout << "BPB_TotSec32 " << BPB_TotSec32 << endl;
    
    theBPB = new BPB(BPB_SecPerClus, BPB_RsvdSecCnt, BPB_NumFATs, BPB_RootEntCnt, BPB_FATSz16, BPB_TotSec32);
    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);

    MachineResumeSignals(&sigState);
}

void storeFAT(int fd){
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    void *sectorData;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sectorData);

    int sectorNumber = 1;
    int size = theBPB->BPB_FATSz16;
    for(int i = 0; i < size; i++){
        readSector(fd, (char*)sectorData, sectorNumber);
        for(int j = 0; j < 512; j += 2){
            FAT.push_back(*(uint16_t *)((char*)sectorData + j));
        }

        sectorNumber++;
    }

    int k = 0;
    for(int i = 0; i < 16; i++){
        for(int j = 0; j < 8; j++){
            printf("%x2", FAT[k]);
            k++;
        }
        cout << endl;
    }

    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
    MachineResumeSignals(&sigState);
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
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

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

    MachineResumeSignals(&sigState);
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
        *pointer = memoryLocation;
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
        memoryPoolWaitQueue.push(*threadVector[CURRENT_THREAD]);
        Scheduler(6,CURRENT_THREAD);
        sharedMemory = threadVector[CURRENT_THREAD]->getSharedMemoryPointer();
    }

    memcpy((char*)sharedMemory, (const char *)data, *length);
    int writeLength;
    int cumLength = 0;
    
    char* writeMemory = (char*)sharedMemory;
    while (*length != 0) {
        if(*length > 512) {
            writeLength = 512;
        }
        else {
            writeLength = *length;
        }
        MachineFileWrite(filedescriptor, (char*)writeMemory, writeLength, callbackMachineFile, &savedCURRENTTHREAD);
        Scheduler(6,CURRENT_THREAD);

        if (threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult() < 0) {
            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sharedMemory);
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        cumLength += threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();

        *length -= writeLength;
        writeMemory = (char*)sharedMemory + writeLength;
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
    char* readMemory = (char*)sharedMemory;

    
    while (*length != 0) {
        if(*length > 512) {
            readLength = 512;
        }
        else {
            readLength = *length;
        }
        MachineFileRead(filedescriptor, (char*)readMemory, readLength, callbackMachineFile, &savedCURRENTTHREAD);
        Scheduler(6,CURRENT_THREAD);
        
        int resultLength = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
        
        if (resultLength < 0) {
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        memcpy((char*)data, (char*)readMemory, resultLength);
        cumLength += resultLength;
        *length -= readLength;
        readMemory = (char*)readMemory + readLength;
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
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

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

    MachineResumeSignals(&sigState);
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
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

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


    MachineResumeSignals(&sigState);
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

// NOTES
/*
 You don't really care about the particular number returned for the FAT image, it isn't going to be visible to the app. Assigning it to some global "FAT_IMAGE_FILE_DESCRIPTOR" would probably be fine.
 
 */

