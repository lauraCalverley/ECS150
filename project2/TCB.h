#ifndef TCB_HEADER
#define TCB_HEADER

#include "VirtualMachine.h"
#include "Machine.h"

class TCB {
    TVMThreadID threadID;
    
    char *stackPointer;
	TVMMemorySize stackSize; // FIXME??? should it be of size_t???
   
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

    TCB();
    TCB(char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContextRef c);
    
    TVMThreadID getThreadID();
    void setThreadID(TVMThreadID id);
    
    
    TVMThreadEntry getTVMThreadEntry();
    void setTVMThreadEntry(TVMThreadEntry e);

    
    TVMThreadState getTVMThreadState();
    void setTVMThreadState(TVMThreadState s);
    
    TVMThreadPriority getTVMThreadPriority();
    void setTVMThreadPriority(TVMThreadPriority p);

    
    int getDeleted();
    void setDeleted(int i);

    
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
