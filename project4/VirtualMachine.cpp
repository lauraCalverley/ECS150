#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include "Mutex.h"
#include "MemoryPool.h"
#include "BPB.h"
#include "Entry.h"
#include <cstring>
#include <vector>
#include <queue>
#include <stdint.h>

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
vector<Entry*> ROOT;
vector<Entry*> openEntries;
int FAT_IMAGE_FILE_DESCRIPTOR;
int NEXT_FILE_DESCRIPTOR = 3;

char CURRENT_PATH[VM_FILE_SYSTEM_MAX_PATH] = "/";
int CURRENT_PATH_SECTOR;

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
void storeRoot(int fd);
int findCluster(int currentClusterNumber, int clustersToHop);
int getPathSectorNumber(char* path);

    
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
        FAT_IMAGE_FILE_DESCRIPTOR = threadVector[savedCurrentThread]->getMachineFileFunctionResult();
        
        if (FAT_IMAGE_FILE_DESCRIPTOR < 0) {
            return VM_STATUS_FAILURE;
        }
        
     
        //save root entry; DON'T PUSH TO FAT.ima
        SVMDirectoryEntry newDirEntry;
        char root[5] = "ROOT";
        // use SFN algorithm to generate DShortFileName for newDirEntry from filename
        memcpy(newDirEntry.DShortFileName, root, 5);
            
        newDirEntry.DSize = 0;
        newDirEntry.DAttributes = 0x10;
        SVMDateTime date;
        VMDateTime(&date);
        newDirEntry.DCreate = date;
        newDirEntry.DAccess = date;
        newDirEntry.DModify = date;   
        Entry* newEntry = new Entry(newDirEntry, 0);
        ROOT.push_back(newEntry);
        ROOT[0]->fileOffset = 1;
        
        
        storeBPB(FAT_IMAGE_FILE_DESCRIPTOR);
        storeFAT(FAT_IMAGE_FILE_DESCRIPTOR);
        storeRoot(FAT_IMAGE_FILE_DESCRIPTOR);

        CURRENT_PATH_SECTOR = theBPB->FirstRootSector;  


        void MachineFileWrite(int fd, void *data, int length, TMachineFileCallback callback, void *calldata);
        void MachineFileSeek(int fd, int offset, int whence, TMachineFileCallback callback, void *calldata);  

        //TVMThreadID savedCurrentThread = CURRENT_THREAD;    
        
        //write FAT
        // MachineFileSeek(FAT_IMAGE_FILE_DESCRIPTOR, theBPB->BPB_RsvdSecCnt * theBPB->BPB_BytsPerSec, 0, callbackMachineFile, &savedCurrentThread);
        // Scheduler(6, savedCurrentThread);
        // MachineFileWrite(FAT_IMAGE_FILE_DESCRIPTOR, theBPB, writeLength, callbackMachineFile, &savedCurrentThread);
        // Scheduler(6, savedCurrentThread);

//         //write ROOT
//         ROOT.erase(ROOT.begin());
//         int sectorSize = theBPB->BPB_BytsPerSec;
//         void* sectorData;
//         VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorSize, &sectorData);
//         int offset = 0;
//         readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, theBPB->FirstRootSector);
//         while(1){
//             if((offset * 32 > theBPB->FirstDataSector) || ROOT.empty()) break;
//             char temp[10];
//             memcpy(temp, (char*)sectorData + ((offset * 32) % sectorSize), 10);
//             SVMDirectoryEntry entry;

//             if(temp[0] == 0x00){
//                 //write remaining root entries
//                 break;
//             }
//             memcpy(&entry.DAttributes, (char *)sectorData + ((offset * 32) % sectorSize) + 11, 1);
//             if ((entry.DAttributes & 0x0F) == 0x0F) { // LFN
//                 offset++;
//                 continue;
//             }
//             else{ //SFN
//                 char *namePtr;
//                 char *extPtr;
                
//                 char fileName[9] = "";
//                 char *dummy1;
//                 char *dummy2;
//                 memcpy(fileName, (char *)sectorData+ ((offset * 32) % sectorSize) , 8);
//                 fileName[8] = '\0';
// //                        cout << "filename is: " << (char*)fileName << endl;
//                 namePtr = strtok_r(fileName, " ", &dummy1);
// //                        cout << "namePtr: " << namePtr << endl;
//                 if(namePtr == '\0'){ // valid SHORT entry
//                     offset++;
//                     continue;
//                 }
//                 char fileExt[4] = "";
//                 memcpy(fileExt, (char *)sectorData+ ((offset * 32) % sectorSize) +8, 3);
//                 fileExt[3] = '\0';
//                 if (fileExt[0] != ' ') {
//                     extPtr = strtok_r(fileExt, " ", &dummy2); // returns a ptr that points to the first byte of the file extension
//                     if(extPtr != '\0'){
//                         strcat(namePtr, ".");
//                         strcat(namePtr, extPtr);
//                     }
//                 }
//                 for(int i = 0; i < ROOT.size(); i++){
//                     if(!strcmp(namePtr, ROOT[i]->e.DShortFileName)){

                        
//                         MachineFileSeek(FAT_IMAGE_FILE_DESCRIPTOR, theBPB->FirstRootSector * theBPB->BPB_BytsPerSec + offset * 32, 0, callbackMachineFile, &savedCurrentThread);
//                         Scheduler(6, savedCurrentThread);
//                         MachineFileWrite(FAT_IMAGE_FILE_DESCRIPTOR, theBPB, writeLength, callbackMachineFile, &savedCurrentThread);
//                         Scheduler(6, savedCurrentThread);
//                     }
//                 }
//             }
//         }


        //write updated data
        
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
    
    MachineResumeSignals(&sigState);
}

void storeBPB(int fd) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    void *sectorData;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sectorData);
    readSector(fd, (char*)sectorData, 0);

    uint16_t BPB_BytsPerSec = *(uint16_t *)((char*)sectorData + 11); 
    uint8_t BPB_SecPerClus = *(uint8_t *)((char*)sectorData + 13); 
    uint16_t BPB_RsvdSecCnt = *(uint16_t *)((char*)sectorData + 14);
    uint8_t BPB_NumFATs = *(uint8_t *)((char*)sectorData + 16); 
    uint16_t BPB_RootEntCnt = *(uint16_t *)((char*)sectorData + 17);
    uint16_t BPB_FATSz16 = *(uint16_t *)((char*)sectorData + 22);   
    uint32_t BPB_TotSec32 = *(uint32_t *)((char*)sectorData + 32); 
    
    theBPB = new BPB(BPB_BytsPerSec, BPB_SecPerClus, BPB_RsvdSecCnt, BPB_NumFATs, BPB_RootEntCnt, BPB_FATSz16, BPB_TotSec32);
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

    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
    MachineResumeSignals(&sigState);
}

void storeRoot(int fd){
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    void *sectorData;
    VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, 512, &sectorData);

    SVMDirectoryEntryRef entry;
    Entry *theEntry;
    for(int sectorNumber = theBPB->FirstRootSector; sectorNumber < theBPB->FirstDataSector; sectorNumber++){
        readSector(fd, (char*)sectorData, sectorNumber);
        
        for (int j = 0; j < theBPB->BPB_BytsPerSec; j+= 32){ // j is the starting byte of the entry
            entry = new SVMDirectoryEntry;
            memcpy(&entry->DAttributes, (char *)sectorData+j+11, 1);
            
            if ((entry->DAttributes & 0x0F) != 0x0F) {
                char *namePtr;
                char *extPtr;
                
                char fileName[9] = "";
                char *dummy1;
                char *dummy2;
                memcpy(fileName, (char *)sectorData+j, 8);
                fileName[8] = '\0';
                namePtr = strtok_r(fileName, " ", &dummy1); // returns a ptr that points to the first byte of the file extension
                if(namePtr != '\0'){ // valid SHORT entry
                    char fileExt[4] = "";
                    memcpy(fileExt, (char *)sectorData+j+8, 3);
                    fileExt[3] = '\0';
                    if (fileExt[0] != ' ') {
                        extPtr = strtok_r(fileExt, " ", &dummy2); // returns a ptr that points to the first byte of the file extension
                        if(extPtr != '\0'){
                            strcat(namePtr, ".");
                            strcat(namePtr, extPtr);
                        }
                    }
                    memcpy(entry->DShortFileName, namePtr, strlen(namePtr)+1);
                    memcpy(entry->DLongFileName, namePtr, strlen(namePtr)+1);
                    
                    memcpy(&(entry->DSize), (char *)sectorData+j+28, 4);

                    SVMDateTime create;
                    SVMDateTime access;
                    SVMDateTime modify;
                    
                    uint16_t createDate;
                    memcpy(&createDate, (char *)sectorData+j+16, 2);

                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    create.DDay = createDate & 0x001F; // or 0x001F
                    create.DMonth = (createDate & 0x01E0) >> 5;
                    create.DYear = ((createDate & 0xFE00) >> 9)+ 1980;
                    uint16_t time;
                    memcpy(&time, (char *)sectorData+j+14, 2);
                    create.DSecond = (time & 0x1F) * 2;
                    create.DMinute = (time & 0x7E0) >> 5;
                    create.DHour = (time & 0xF800) >> 11;
                    
                    memcpy(&create.DHundredth, (char *)sectorData+j+13, 1); // add 1 second if necessary then % 100
                    
                    create.DSecond += (create.DHundredth / 100);
                    create.DHundredth = create.DHundredth % 100;
                           
                    uint16_t accessDate;
                    memcpy(&accessDate, (char *)sectorData+j+18, 2);
                    
                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    access.DDay = accessDate & 0x001F; // or 0x001F
                    access.DMonth = (accessDate & 0x01E0) >> 5;
                    access.DYear = ((accessDate & 0xFE00) >> 9)+ 1980;

                    access.DSecond = 0;
                    access.DMinute = 0;
                    access.DHour = 0;
                    access.DHundredth = 0;
                    
                    uint16_t modifyDate;
                    memcpy(&modifyDate, (char *)sectorData+j+24, 2);
                    
                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    modify.DDay = modifyDate & 0x001F; // or 0x001F
                    modify.DMonth = (modifyDate & 0x01E0) >> 5;
                    modify.DYear = ((modifyDate & 0xFE00) >> 9)+ 1980;
                    uint16_t modifyTime;
                    memcpy(&modifyTime, (char *)sectorData+j+22, 2);
                    modify.DSecond = (modifyTime & 0x1F) * 2;
                    modify.DMinute = (modifyTime & 0x7E0) >> 5;
                    modify.DHour = (modifyTime & 0xF800) >> 11;
                    
                    modify.DHundredth = 0;

                    entry->DCreate = create;
                    entry->DAccess = access;
                    entry->DModify = modify;
                    
                    uint16_t firstClusterStart;
                    memcpy(&firstClusterStart, (char *)sectorData+j+26, 2);
                    
                    theEntry = new Entry(*entry, firstClusterStart); 

                    ROOT.push_back(theEntry);
                    
                }
            }
        }
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

    if(filedescriptor < 3){

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
    else{
        int lengthToWrite = *length; // allows us to update length with bytes actually read as we go 
        for (int i=0; i < openEntries.size(); i++) {
            if ((openEntries[i]->descriptor) == filedescriptor) { //find matching file -- ROOT[i]

                //check for write permission
                if(openEntries[i]->writeable == 0){
                    *length = 0;
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_FAILURE;
                }          
                
                int sectorSize = theBPB->BPB_BytsPerSec;
                void *sectorData;
                VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorSize, &sectorData);

                int offset = openEntries[i]->fileOffset; // # bytes
                int currentClusterNumber = openEntries[i]->firstClusterNumber; //starting cluster w/out offset

                // determine # of sectors to write to
                int sectorCount = 1;
                for (int i=0; i < offset+lengthToWrite; i += sectorSize) {
                    if (offset < i) {
                        sectorCount++;
                    }
                }
                
                //create a vector of all sector numbers that need to be written to
                vector<int> sectorsToWrite;
                int currentSector = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (offset / sectorSize);
                for(int j = 0; j < sectorCount; j++){
                    sectorsToWrite.push_back(currentSector);
                    currentSector++;
                    if(((currentSector + 1) % theBPB->BPB_SecPerClus) == 0){
                        currentClusterNumber = findCluster(currentClusterNumber, 1);
                        currentSector = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (offset / sectorSize);
                    }
                }

                //write to sectors and push to dirtySectors
                int dataPosition = 0;
                bool sectorIsDirty;
                for(int j = 0; j < sectorsToWrite.size(); j++){
                    sectorIsDirty = 0;

                    //check if its already dirty
                    for(int k = 0; k < openEntries[i]->dirtySectors.size(); k++){
                        //if dirty sector is found write to it
                        if(sectorsToWrite[j] == openEntries[i]->dirtySectors[k].sectorNumber){
                            //write to dirty sector at offset
                            int writeSize = lengthToWrite < (sectorSize - offset) ? lengthToWrite : (sectorSize - offset);
                            memcpy(openEntries[i]->dirtySectors[k].data + (offset % sectorSize), (char*)data + dataPosition, writeSize);
                            offset = 0;

                            //adjust lengthToWrite
                            lengthToWrite -= writeSize;

                            //adjust data position
                            dataPosition += writeSize;

                            *length = dataPosition;
                            sectorIsDirty = 1;
                            break;
                        }
                    }

                    //if dirty sector was not found, read in sector, write to it, and push to dirty sector
                    if(sectorIsDirty == 0){
                        //read in data sector
                        readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, sectorsToWrite[j]);
                        
                        //write to sector at offset
                        int writeSize = (lengthToWrite < (sectorSize - offset)) ? lengthToWrite : (sectorSize - offset);
                        memcpy((char*)sectorData + (offset % sectorSize), (char*)data + dataPosition, writeSize);
                        offset = 0;

                        //push to dirtySector
                        Entry::Sector tempSector;
                        memcpy(tempSector.data, (char*)sectorData, sectorSize);
                        
                        tempSector.sectorNumber = sectorsToWrite[j];
                        openEntries[i]->dirtySectors.push_back(tempSector);

                        //adjust lengthToWrite
                        lengthToWrite -= writeSize;

                        //adjust data position
                        dataPosition += writeSize;

                        *length = dataPosition;
                    }
                }
                openEntries[i]->e.DSize = openEntries[i]->fileOffset;
                
                if ((openEntries[i]->fileOffset + *length) > openEntries[i]->e.DSize) {
                    openEntries[i]->e.DSize = openEntries[i]->fileOffset + *length;
                }
                
                openEntries[i]->fileOffset += *length;
                
                VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);

                MachineResumeSignals(&sigState);
                return VM_STATUS_SUCCESS;
            }
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if ((filename==NULL) || (filedescriptor==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    bool fileFound = 0;
    int currentCluster;
    int sectorSize = theBPB->BPB_BytsPerSec;
    

    if(!strcmp(CURRENT_PATH, "/")){ // in root directory
        for (int i=0; i < ROOT.size(); i++) {
            if (strcmp((ROOT[i]->e.DShortFileName),filename) == 0) { //find matching file -- ROOT[i]
                
                if (ROOT[i]->directory) { // this is a directory, not a file
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_FAILURE;
                }

                currentCluster = ROOT[i]->firstClusterNumber;
                // check for corruption - start with (ROOT[i]->firstClusterNumber and follow the FAT cells till 0xFFFF; corrupted = 0xFFF7
                while (currentCluster != 0xFFFF) {
                    if (currentCluster == 0xFFF7) {
                        MachineResumeSignals(&sigState);
                        return VM_STATUS_FAILURE; // file is corrupted - do not open
                    }
                    else {
                        currentCluster = FAT[currentCluster];
                    }
                }
                
                if((flags & O_RDWR) == O_RDWR){
                    ROOT[i]->writeable = 1;
                }
                else{
                    ROOT[i]->writeable = 0;
                }
                
                // create and save file descriptor
                ROOT[i]->descriptor = NEXT_FILE_DESCRIPTOR++;
                openEntries.push_back(ROOT[i]);
                
                
                *filedescriptor = ROOT[i]->descriptor;
                fileFound = 1;
                break;
            }
        }
        //if file not found create it
        if(fileFound == 0){
            //create SVMDirectoryEntry
            SVMDirectoryEntry newDirEntry;
            
            // use SFN algorithm to generate DShortFileName for newDirEntry from filename
            memcpy(newDirEntry.DShortFileName, filename, strlen(filename) + 1);
            memcpy(newDirEntry.DLongFileName, filename, strlen(filename) + 1);
                
            newDirEntry.DSize = 0;
            newDirEntry.DAttributes = 0x00;
            SVMDateTime date;
            if(VM_STATUS_SUCCESS != VMDateTime(&date)){
                MachineResumeSignals(&sigState);
                return VM_STATUS_FAILURE;
            }
             
            newDirEntry.DCreate = date;
            newDirEntry.DAccess = date;
            newDirEntry.DModify = date;    
            
            //find first free cluster number and replace fat with fff8
            int clusterNum = 0;
            for(int i = 0; i < FAT.size(); i++){
                if(FAT[i] == 0x0000){
                    clusterNum = i;
                    FAT[i] = 0xfff8;
                    break;
                }
            }
                    
            Entry* newEntry = new Entry(newDirEntry, clusterNum, NEXT_FILE_DESCRIPTOR++);
            if((flags & O_RDWR) == O_RDWR){
                newEntry->writeable = 1;
            }
            else{
                newEntry->writeable = 0;
            }
            
            ROOT.push_back(newEntry);
            openEntries.push_back(newEntry);
            *filedescriptor = ROOT[ROOT.size()-1]->descriptor;
        }
    }
    else{//not in ROOT directory
        int currentClusterNumber = ROOT[1]->firstClusterNumber;
        int currentSector = getPathSectorNumber(CURRENT_PATH);
        void* sectorData;
        VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, theBPB->BPB_BytsPerSec, &sectorData);
        readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, currentSector);

        int offset = 0;
        while (1) {
            char temp[10];
            memcpy(temp, (char*)sectorData + ((offset * 32) % sectorSize), 10);
            SVMDirectoryEntry entry;

            if(temp[0] == 0x00){
                offset = 0;
                MachineResumeSignals(&sigState);
                return VM_STATUS_FAILURE;
            }
            memcpy(&entry.DAttributes, (char *)sectorData + ((offset * 32) % sectorSize) + 11, 1);
        
            if ((entry.DAttributes & 0x0F) == 0x0F) { // LFN
                offset++;
                if((offset * 32 / sectorSize) >= 1){
                    currentClusterNumber = findCluster(currentClusterNumber, 1);
                    currentSector = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (offset / sectorSize);
                    readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, currentSector);
                }
            }
            else { // SFN
                char *namePtr;
                char *extPtr;
                
                char fileName[9] = "";
                char *dummy1;
                char *dummy2;
                memcpy(fileName, (char *)sectorData+ ((offset * 32) % sectorSize) , 8);
                fileName[8] = '\0';
                namePtr = strtok_r(fileName, " ", &dummy1);
                if(namePtr != '\0'){ // valid SHORT entry
                    char fileExt[4] = "";
                    memcpy(fileExt, (char *)sectorData+ ((offset * 32) % sectorSize) +8, 3);
                    fileExt[3] = '\0';
                    if (fileExt[0] != ' ') {
                        extPtr = strtok_r(fileExt, " ", &dummy2); // returns a ptr that points to the first byte of the file extension
                        if(extPtr != '\0'){
                            strcat(namePtr, ".");
                            strcat(namePtr, extPtr);
                        }
                    }

                    if(!strcmp(namePtr,filename)) {
                        offset++;
                        break;
                    }
                    memcpy(entry.DShortFileName, namePtr, strlen(namePtr)+1);
                    memcpy(entry.DLongFileName, namePtr, strlen(namePtr)+1);
                    
                    memcpy(&(entry.DSize), (char *)sectorData+ ((offset * 32) % sectorSize) +28, 4);
                    
                    SVMDateTime create;
                    SVMDateTime access;
                    SVMDateTime modify;
                    
                    uint16_t createDate;
                    memcpy(&createDate, (char *)sectorData+ ((offset * 32) % sectorSize) +16, 2);
                    
                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    create.DDay = createDate & 0x001F; // or 0x001F
                    create.DMonth = (createDate & 0x01E0) >> 5;
                    create.DYear = ((createDate & 0xFE00) >> 9)+ 1980;
                    uint16_t time;
                    memcpy(&time, (char *)sectorData+ ((offset * 32) % sectorSize) +14, 2);
                    create.DSecond = (time & 0x1F) * 2;
                    create.DMinute = (time & 0x7E0) >> 5;
                    create.DHour = (time & 0xF800) >> 11;
                    
                    memcpy(&create.DHundredth, (char *)sectorData+ ((offset * 32) % sectorSize) +13, 1); // add 1 second if necessary then % 100
                    
                    create.DSecond += (create.DHundredth / 100);
                    create.DHundredth = create.DHundredth % 100;
                    
                    
                    uint16_t accessDate;
                    memcpy(&accessDate, (char *)sectorData+ ((offset * 32) % sectorSize) +18, 2);
                    
                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    access.DDay = accessDate & 0x001F; // or 0x001F
                    access.DMonth = (accessDate & 0x01E0) >> 5;
                    access.DYear = ((accessDate & 0xFE00) >> 9)+ 1980;
                    
                    access.DSecond = 0;
                    access.DMinute = 0;
                    access.DHour = 0;
                    access.DHundredth = 0;
                    
                    uint16_t modifyDate;
                    memcpy(&modifyDate, (char *)sectorData+ ((offset * 32) % sectorSize) +24, 2);
                    
                    // 0-4 day of month: range 1-31
                    // 5-8 month of year: range 1-12
                    // 9-15 number of years since 1980: range 0-127
                    modify.DDay = modifyDate & 0x001F; // or 0x001F
                    modify.DMonth = (modifyDate & 0x01E0) >> 5;
                    modify.DYear = ((modifyDate & 0xFE00) >> 9)+ 1980;
                    uint16_t modifyTime;
                    memcpy(&modifyTime, (char *)sectorData+ ((offset * 32) % sectorSize) +22, 2);
                    modify.DSecond = (modifyTime & 0x1F) * 2;
                    modify.DMinute = (modifyTime & 0x7E0) >> 5;
                    modify.DHour = (modifyTime & 0xF800) >> 11;
                    
                    modify.DHundredth = 0;
                    
                    entry.DCreate = create;
                    entry.DAccess = access;
                    entry.DModify = modify;
                    
                    uint16_t firstClusterStart;
                    memcpy(&firstClusterStart, (char *)sectorData+ ((offset * 32) % sectorSize) +26, 2);

                    //find first free cluster number and replace fat with fff8
                    int clusterNum = 0;
                    for(int j = 0; j < FAT.size(); j++){
                        if(FAT[j] == 0x0000){
                            clusterNum = j;
                            FAT[j] = 0xfff8;
                            break;
                        }
                    }

                    Entry* theEntry = new Entry(entry, clusterNum, NEXT_FILE_DESCRIPTOR++);
                    openEntries.push_back(theEntry);
                    *filedescriptor = openEntries[openEntries.size() - 1]->descriptor;
                    VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_SUCCESS;
                }
                else {
                    offset++;
                }
            }
        }



    }

    
    if (*filedescriptor < 3) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

int getPathSectorNumber(char* path){
    char* saveptr;
    char* token = strtok_r(path, "/", &saveptr);
    int result;
    int apps;
    for(int i = 1; i < ROOT.size(); i++){
        if(!strcmp(ROOT[i]->e.DShortFileName, "APPS")){
            apps = (ROOT[i]->firstClusterNumber - 2) * theBPB->BPB_SecPerClus + theBPB->FirstDataSector;
            break;
        }
    }

    while(token != NULL){
        if(!strcmp(token, "APPS")){
            result = apps; 
        }
        else if(!strcmp(token, "..")){
            result = theBPB->FirstRootSector;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }
    return result;
}

TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if ((dirname==NULL) || (dirdescriptor==NULL)) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    bool dirFound = 0;
    int currentCluster;
    if(!strcmp(dirname, "/")){
        ROOT[0]->descriptor = NEXT_FILE_DESCRIPTOR++;
        openEntries.push_back(ROOT[0]);
        *dirdescriptor = openEntries[openEntries.size() - 1]->descriptor;
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
    char name[VM_FILE_SYSTEM_MAX_PATH];
    VMFileSystemFileFromFullPath(name, dirname);
    for (int i=0; i < ROOT.size(); i++) {
        if (strcmp((ROOT[i]->e.DShortFileName),name) == 0) { //find matching dir -- ROOT[i]
            
            currentCluster = ROOT[i]->firstClusterNumber;
            // check for corruption - start with (ROOT[i]->firstClusterNumber and follow the FAT cells till 0xFFFF; corrupted = 0xFFF7
            while (currentCluster != 0xFFFF) {
                if (currentCluster == 0xFFF7) {
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_FAILURE; // file is corrupted - do not open
                }
                else {
                    currentCluster = FAT[currentCluster];
                }
            }
            
            // create and save file descriptor
            ROOT[i]->descriptor = NEXT_FILE_DESCRIPTOR++;
            openEntries.push_back(ROOT[i]);
            
            *dirdescriptor = ROOT[i]->descriptor;
            dirFound = 1;
            break;
        }
    }
    
    //if dir not found create it
    if(dirFound == 0){
        *dirdescriptor = -1;
    }
    
    if (*dirdescriptor < 3) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else {
        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

    
    
TVMStatus VMDirectoryCurrent(char *abspath) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (abspath==NULL) {
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    VMFileSystemGetAbsolutePath(abspath, CURRENT_PATH, ".");
    MachineResumeSignals(&sigState);
    return VM_STATUS_SUCCESS;



}

TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent){
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    if(dirent == NULL){
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
 
    int sectorSize = theBPB->BPB_BytsPerSec;
    for (int i=0; i < openEntries.size(); i++) {
        if ((openEntries[i]->descriptor) == dirdescriptor) { //find matching directory -- ROOT[i]
            //root directory
            if (strcmp((openEntries[i]->e.DShortFileName),"ROOT") == 0) { //find matching entry
                if(ROOT[0]->fileOffset >= ROOT.size()){
                    ROOT[0]->fileOffset = 1; // reset
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_FAILURE;
                }
                *dirent = ROOT[ROOT[0]->fileOffset]->e;
                ROOT[0]->fileOffset++;
                MachineResumeSignals(&sigState);
                return VM_STATUS_SUCCESS;                
            }
            else { //not in root
                void *sectorData;
                VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, theBPB->BPB_BytsPerSec, &sectorData);
                
                
                SVMDirectoryEntry entry; // NOT a pointer, so: *dirent = entry;

                int currentClusterNumber = openEntries[i]->firstClusterNumber;
                int sectorToRead = CURRENT_PATH_SECTOR;
                int sectorsToHop = openEntries[i]->fileOffset * 32 / sectorSize;
                for(int j = 0; j < sectorsToHop; j++){
                    sectorToRead++;
                    if(((sectorToRead + 1) % theBPB->BPB_SecPerClus) == 0){
                        currentClusterNumber = findCluster(currentClusterNumber, 1);
                        sectorToRead = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (openEntries[i]->fileOffset / sectorSize);
                    }
                }

                readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, sectorToRead);
                while (1) {
                    char temp[10];
                    memcpy(temp, (char*)sectorData + ((openEntries[i]->fileOffset * 32) % sectorSize), 10);

                    if(temp[0] == 0x00){
                        openEntries[i]->fileOffset = 0;
                        MachineResumeSignals(&sigState);
                        return VM_STATUS_FAILURE;
                    }
                    memcpy(&entry.DAttributes, (char *)sectorData + ((openEntries[i]->fileOffset * 32) % sectorSize) + 11, 1);
                
                    if ((entry.DAttributes & 0x0F) == 0x0F) { // LFN
                        openEntries[i]->fileOffset++;
                        if((openEntries[i]->fileOffset * 32 / sectorSize) >= 1){
                            currentClusterNumber = findCluster(currentClusterNumber, 1);
                            sectorToRead = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (openEntries[i]->fileOffset / sectorSize);
                            readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, sectorToRead);
                        }
                    }
                    else { // SFN
                        char *namePtr;
                        char *extPtr;
                        
                        char fileName[9] = "";
                        char *dummy1;
                        char *dummy2;
                        memcpy(fileName, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) , 8);
                        fileName[8] = '\0';
                        namePtr = strtok_r(fileName, " ", &dummy1);
                        if(namePtr != '\0'){ // valid SHORT entry
                            char fileExt[4] = "";
                            memcpy(fileExt, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +8, 3);
                            fileExt[3] = '\0';
                            if (fileExt[0] != ' ') {
                                extPtr = strtok_r(fileExt, " ", &dummy2); // returns a ptr that points to the first byte of the file extension
                                if(extPtr != '\0'){
                                    strcat(namePtr, ".");
                                    strcat(namePtr, extPtr);
                                }
                            }
                            memcpy(entry.DShortFileName, namePtr, strlen(namePtr)+1);
                            memcpy(entry.DLongFileName, namePtr, strlen(namePtr)+1);
                            
                            memcpy(&(entry.DSize), (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +28, 4);
                            
                            SVMDateTime create;
                            SVMDateTime access;
                            SVMDateTime modify;
                            
                            uint16_t createDate;
                            memcpy(&createDate, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +16, 2);
                            
                            // 0-4 day of month: range 1-31
                            // 5-8 month of year: range 1-12
                            // 9-15 number of years since 1980: range 0-127
                            create.DDay = createDate & 0x001F; // or 0x001F
                            create.DMonth = (createDate & 0x01E0) >> 5;
                            create.DYear = ((createDate & 0xFE00) >> 9)+ 1980;
                            uint16_t time;
                            memcpy(&time, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +14, 2);
                            create.DSecond = (time & 0x1F) * 2;
                            create.DMinute = (time & 0x7E0) >> 5;
                            create.DHour = (time & 0xF800) >> 11;
                            
                            memcpy(&create.DHundredth, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +13, 1); // add 1 second if necessary then % 100
                            
                            create.DSecond += (create.DHundredth / 100);
                            create.DHundredth = create.DHundredth % 100;
                            
                            uint16_t accessDate;
                            memcpy(&accessDate, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +18, 2);
                            
                            // 0-4 day of month: range 1-31
                            // 5-8 month of year: range 1-12
                            // 9-15 number of years since 1980: range 0-127
                            access.DDay = accessDate & 0x001F; // or 0x001F
                            access.DMonth = (accessDate & 0x01E0) >> 5;
                            access.DYear = ((accessDate & 0xFE00) >> 9)+ 1980;
                            
                            access.DSecond = 0;
                            access.DMinute = 0;
                            access.DHour = 0;
                            access.DHundredth = 0;
                            
                            uint16_t modifyDate;
                            memcpy(&modifyDate, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +24, 2);
                            
                            // 0-4 day of month: range 1-31
                            // 5-8 month of year: range 1-12
                            // 9-15 number of years since 1980: range 0-127
                            modify.DDay = modifyDate & 0x001F; // or 0x001F
                            modify.DMonth = (modifyDate & 0x01E0) >> 5;
                            modify.DYear = ((modifyDate & 0xFE00) >> 9)+ 1980;
                            uint16_t modifyTime;
                            memcpy(&modifyTime, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +22, 2);
                            modify.DSecond = (modifyTime & 0x1F) * 2;
                            modify.DMinute = (modifyTime & 0x7E0) >> 5;
                            modify.DHour = (modifyTime & 0xF800) >> 11;
                            
                            modify.DHundredth = 0;
                            
                            entry.DCreate = create;
                            entry.DAccess = access;
                            entry.DModify = modify;
                            
                            uint16_t firstClusterStart;
                            memcpy(&firstClusterStart, (char *)sectorData+ ((openEntries[i]->fileOffset * 32) % sectorSize) +26, 2);
                            
                            *dirent = entry;
                            openEntries[i]->fileOffset++;
                            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
                            MachineResumeSignals(&sigState);
                            return VM_STATUS_SUCCESS;
                        }
                        else {
                            openEntries[i]->fileOffset++;
                        }
                    }
                } // while
                VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
            } //else
            break;
        } // if
    } // for
    MachineResumeSignals(&sigState);
    return VM_STATUS_ERROR_INVALID_PARAMETER;
}
    
    
TVMStatus VMDirectoryClose(int dirdescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    for (int i=0; i < openEntries.size(); i++) {
        if ((openEntries[i]->descriptor) == dirdescriptor) { //found matching directory
            openEntries[i]->descriptor = -1;
            openEntries.erase(openEntries.begin()+i);
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }

    MachineResumeSignals(&sigState);
    return VM_STATUS_FAILURE;
}
    
    
    
TVMStatus VMDirectoryRewind(int dirdescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    for (int i=0; i < openEntries.size(); i++) {
        if ((openEntries[i]->descriptor) == dirdescriptor) { //found matching directory
            if (strcmp((openEntries[i]->e.DShortFileName),"ROOT") == 0) { //find matching entry
                ROOT[i]->fileOffset = 1;
            }
            else {
                ROOT[i]->fileOffset = 0;
            }
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }    
    
    MachineResumeSignals(&sigState);
    return VM_STATUS_FAILURE;
}
    
    
TVMStatus VMDirectoryChange(const char *path) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);

    if(path == NULL){
        MachineResumeSignals(&sigState);
        return VM_STATUS_ERROR_INVALID_PARAMETER;       
    }    
    char* saveptr;
    char* token = strtok_r((char*)path, "/", &saveptr);
    if(!strcmp(CURRENT_PATH, "/")){ //in root directory
        for(int i = 1; i < ROOT.size(); i++){
            if(!strcmp(token, ROOT[i]->e.DShortFileName)){
                strcat(CURRENT_PATH, token);
                CURRENT_PATH_SECTOR = theBPB->FirstDataSector + ((ROOT[i]->firstClusterNumber - 2) * 2);
                token = strtok_r(NULL, "/", &saveptr);
                if(token == NULL){
                    MachineResumeSignals(&sigState);
                    return VM_STATUS_SUCCESS;
                }
            }
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
    else { //not in root
        char temp[VM_FILE_SYSTEM_MAX_PATH];

        if(VM_STATUS_SUCCESS != VMFileSystemGetAbsolutePath(temp, CURRENT_PATH, path)){
            MachineResumeSignals(&sigState);
            return VM_STATUS_FAILURE;
        }
        memcpy(CURRENT_PATH, temp, VM_FILE_SYSTEM_MAX_PATH);

        CURRENT_PATH_SECTOR = theBPB->FirstRootSector;

        MachineResumeSignals(&sigState);
        return VM_STATUS_SUCCESS;
    }
}

    
    
TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    if (filedescriptor < 3) {
            TVMThreadID savedCURRENTTHREAD = CURRENT_THREAD;
            MachineFileSeek(filedescriptor, offset, whence, callbackMachineFile, &savedCURRENTTHREAD);
            Scheduler(6,CURRENT_THREAD);
        
            *newoffset = threadVector[savedCURRENTTHREAD]->getMachineFileFunctionResult();
    }
    else {
        for (int i=0; i < openEntries.size(); i++) {
            if ((openEntries[i]->descriptor) == filedescriptor) { //find matching file -- ROOT[i]
                if ((whence + offset) <= openEntries[i]->e.DSize) {
                    openEntries[i]->fileOffset = whence + offset;
                    *newoffset = openEntries[i]->fileOffset;
                }
                else {
                    *newoffset = -1;
                }
                break;
            }
        }
    }

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
    
    if (filedescriptor < 3) {
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
    else {
        
        int lengthToRead = *length; // allows us to update length with bytes actually read as we go
        
        for (int i=0; i < openEntries.size(); i++) {
            if ((openEntries[i]->descriptor) == filedescriptor) { //find matching file -- ROOT[i]
                
                int sectorSize = theBPB->BPB_BytsPerSec;
                void *sectorData;
                VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorSize, &sectorData);

                int offset = openEntries[i]->fileOffset; // # bytes
                int currentClusterNumber = openEntries[i]->firstClusterNumber;

                if((offset + lengthToRead) > openEntries[i]->e.DSize){
                    lengthToRead = openEntries[i]->e.DSize - offset;
                }

                // determine # of sectors to read in
                int sectorsToRead = 1;
                for (int i=0; i < offset+lengthToRead; i += sectorSize) {
                    if (offset < i) {
                        sectorsToRead++;
                    }
                }

                int currentSector = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (offset / sectorSize);
                char tempData[lengthToRead + 1022];
                tempData[0] = '\0';
                for(int j= 0; j < sectorsToRead; j++){
                    bool sectorIsDirty = 0;
                    //check if its already dirty
                    for(int k = 0; k < openEntries[i]->dirtySectors.size(); k++){
                        //if dirty sector is found write to it
                        if(currentSector == openEntries[i]->dirtySectors[k].sectorNumber){ // currentSector is dirty
                            memcpy((char*)sectorData, openEntries[i]->dirtySectors[k].data, sectorSize);
                            sectorIsDirty = 1;
                            break;
                        }
                    }

                    if (!sectorIsDirty) {
                        readSector(FAT_IMAGE_FILE_DESCRIPTOR, (char*)sectorData, currentSector);
                    }
                    char *dummy = "\0";
                    memcpy((char*)sectorData + sectorSize, dummy, 1);
                    
                    strcat(tempData, (char*)sectorData);
                    
                    currentSector++;
                    if(((currentSector + 1) % theBPB->BPB_SecPerClus) == 0){
                        currentClusterNumber = findCluster(currentClusterNumber, 1);
                        currentSector = theBPB->FirstDataSector + ((currentClusterNumber - 2) * 2) + (offset / sectorSize);
                    }
                }

                // trim edges
                memcpy((char*)data, tempData + (offset % sectorSize), lengthToRead);
                *length = lengthToRead;
                openEntries[i]->fileOffset += lengthToRead;
                
                VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED_MEMORY, sectorData);
                MachineResumeSignals(&sigState);
                return VM_STATUS_SUCCESS;
            }
        }
        MachineResumeSignals(&sigState);
        return VM_STATUS_FAILURE;
    }
}

int findCluster(int currentClusterNumber, int clustersToHop) {
    
    for (int i=0; i < clustersToHop; i++) {
        if (currentClusterNumber == 0xFFF8) { // checks for out-of-bounds
            return -1;
        }
        currentClusterNumber = FAT[currentClusterNumber];
    }
    return currentClusterNumber;
}

    
TVMStatus VMFileClose(int filedescriptor) {
    TMachineSignalState sigState;
    MachineSuspendSignals(&sigState);
    
    for (int i=0; i < openEntries.size(); i++) {
        if ((openEntries[i]->descriptor) == filedescriptor) { //find matching file -- ROOT[i]
            openEntries[i]->descriptor = -1;
            openEntries[i]->fileOffset = 0;
            openEntries.erase(openEntries.begin()+i);
            MachineResumeSignals(&sigState);
            return VM_STATUS_SUCCESS;
        }
    }

    MachineResumeSignals(&sigState);
    return VM_STATUS_FAILURE;
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

