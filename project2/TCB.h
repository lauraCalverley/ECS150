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

    TCB(TVMThreadIDRef tid, char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContextRef c) {
        
        threadID = threadCount;
        *tid = threadID; // FIXME???
        threadCount++;

        stackPointer = stackP;
        stackSize = stackS;
        
        state = s;
        priority = p;
        
        entry = e;
        params = entryParams;
        
        context = c;
        
        deleted = 0;
    }
    
    
    TVMThreadID getThreadID() {
        return threadID;
    }
    void setThreadID(TVMThreadID id) {
        threadID = id;
    }
    
    
    TVMThreadEntry getTVMThreadEntry() {
        return entry;
    }
    void setTVMThreadEntry(TVMThreadEntry e) {
        entry = e;
    }

    
    TVMThreadState getTVMThreadState() {
        return state;
    }
    void setTVMThreadState(TVMThreadState s) {
        state = s;
    }
    
    
    TVMThreadPriority getTVMThreadPriority() {
        return priority;
    }
    void setTVMThreadPriority(TVMThreadPriority p) {
        priority = p;
    }

    
    int getDeleted() {
        return deleted;
    }
    void setDeleted(int i = 1) { // should only be used to mark as deleted because once deleted, would no longer access
        deleted = i;
    }

    
    friend bool operator<(const TCB& lhs, const TCB& rhs)
    {
        if (lhs.priority < rhs.priority) {
            return 1;
        }
        else {
            return 0;
        }
    }
    
};

#endif
