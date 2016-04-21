#ifndef TCB_HEADER
#define TCB_HEADER

#include "VirtualMachine.h"

class TCB {
    unsigned int threadID;
    int *stackPointer; // FIXME change to an actual stack???

    TVMThreadEntry entry; // function // do we need the parameters
    TVMThreadState state;
    TVMThreadPriority priority;
    
    
public:
    
    volatile int SleepCount; // public?
    static int threadCount;

    TCB(TVMThreadEntry e, TVMThreadPriority p) { // FIXME stack pointer/size???
        entry = e;
        priority = p;
        state = VM_THREAD_STATE_DEAD;

        threadCount++;
    }
    
    
    unsigned int getThreadID() {
        return threadID;
    }
    void setThreadID(unsigned int id) {
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

    
    
};

#endif