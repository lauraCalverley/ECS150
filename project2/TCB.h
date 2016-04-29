#ifndef TCB_HEADER
#define TCB_HEADER

#include "VirtualMachine.h"
#include "Machine.h"

class TCB {
    TVMThreadID threadID;
    
    char *stackPointer;
	TVMMemorySize stackSize;
   
    TVMThreadState state;
    TVMThreadPriority priority;
    
    TVMThreadEntry entry;
	void *params;
    
    SMachineContext context;
    
    int deleted;
    volatile int sleepCount;
    volatile int mutexWaitCount;
    int machineFileFunctionResult;
    
public:
    
    TCB() {}
    
    TCB(TVMThreadID tid, char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContext c);
    
    TVMThreadID getThreadID() const;
    TVMThreadIDRef getThreadIDRef();
    void setThreadID(TVMThreadID id);
    
    char* getStackPointer();
    void setStackPointer(char* s);
    
    TVMMemorySize getStackSize();
    void setStackSize(TVMMemorySize s);
    
    
    TVMThreadEntry getTVMThreadEntry();
    void setTVMThreadEntry(TVMThreadEntry e);

    void* getParams();
    void setParams(void* p);
    
    TVMThreadState getTVMThreadState();
    void setTVMThreadState(TVMThreadState s);
    
    TVMThreadPriority getTVMThreadPriority();
    void setTVMThreadPriority(TVMThreadPriority p);

    SMachineContextRef getMachineContext();
    void setMachineContext(SMachineContext c);
    
    int getDeleted();
    void setDeleted(int i=1);

    int getSleepCount();
    void setSleepCount(int ticks);
    void decrementSleepCount();
    
    int getMutexWaitCount();
    void setMutexWaitCount(int ticks);
    void decrementMutexWaitCount();

    int getFileCallbackStatus();
    void setFileCallbackStatus(int status);

    int getMachineFileFunctionResult();
    void setMachineFileFunctionResult(int result);
    
    friend bool operator<(const TCB& lhs, const TCB& rhs){
        if (lhs.priority < rhs.priority) {
            return 1;
        }
        else {
            return 0;
        }
    }
};

#endif
