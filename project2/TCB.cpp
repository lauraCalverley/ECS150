#include "TCB.h"
#include "VirtualMachine.h"
#include "Machine.h"

int TCB::threadCount;

/*TCB::TCB() { // main's TCB
        threadID = threadCount;
        threadCount++;
        
        stackPointer = NULL;
        stackSize = 0;
        
        state = VM_THREAD_STATE_RUNNING;
        priority = VM_THREAD_PRIORITY_NORMAL;
        
        entry = NULL;
        params = NULL;
        
        context = NULL;
        
        deleted = 0;
        
    }*/
    
 
    
TCB::TCB(TVMThreadIDRef tid, char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContext c) {
        
        threadID = threadCount;
        tid = &threadID; // FIXME???
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
    
    
TVMThreadID TCB::getThreadID() {
        return threadID;
}
void TCB::setThreadID(TVMThreadID id) {
        threadID = id;
}
    
char* TCB::getStackPointer() {
    return stackPointer;
}

void TCB::setStackPointer(char* s) {
    stackPointer = s;
}


TVMMemorySize TCB::getStackSize() {
    return stackSize;
}

void TCB::setStackSize(TVMMemorySize s) {
    stackSize = s;
}


TVMThreadEntry TCB::getTVMThreadEntry() {
        return entry;
}
void TCB::setTVMThreadEntry(TVMThreadEntry e) {
        entry = e;
}


void* TCB::getParams() {
    return params;
}


void TCB::setParams(void* p) {
    params = p;
}


TVMThreadState TCB::getTVMThreadState() {
        return state;
}
void TCB::setTVMThreadState(TVMThreadState s) {
        state = s;
}
    
    
TVMThreadPriority TCB::getTVMThreadPriority() {
        return priority;
}
void TCB::setTVMThreadPriority(TVMThreadPriority p) {
        priority = p;
}
    
SMachineContext TCB::getMachineContext() {
    return context;
}

void TCB::setMachineContext(SMachineContext c) {
    context = c;
}



int TCB::getDeleted() {
        return deleted;
}
void TCB::setDeleted(int i) { // should only be used to mark as deleted because once deleted, would no longer access
        deleted = i;
}