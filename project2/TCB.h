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
    
    SMachineContextRef context;// this is a pointer
    
    int deleted;
    
public:
    
    volatile int SleepCount; // public?
    volatile int callbackStatus; // 0 is waiting, 1 is operation done
    static int threadCount;

    //TCB();
    TCB(TVMThreadIDRef tid, char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContextRef c);
    
    TVMThreadID getThreadID();
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
    void setMachineContext(SMachineContextRef c);
    
    int getDeleted();
    void setDeleted(int i=1);

    
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
