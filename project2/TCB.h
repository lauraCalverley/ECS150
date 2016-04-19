#ifndef TCB_HEADER
#define TCB_HEADER

class TCB {
    int threadID;
    int *stackPointer;
    
    enum {
        DEAD,
        WAITING,
        READY,
        RUNNING
    } state;
    
    enum {
        LOW,
        MEDIUM,
        HIGH
    } priorityLevel;

public:
    
    volatile int SleepCount; // public?
    
    int getThreadID() {
        return threadID;
    }
    
    void setThreadID(int id) {
        threadID = id;
    }
    
    
    
    
    
};

#endif