#include "TCB.h"
#include "VirtualMachine.h"
#include "Machine.h"
    
TCB::TCB(TVMThreadID tid, char *stackP, TVMMemorySize stackS, TVMThreadState s, TVMThreadPriority p, TVMThreadEntry e, void* entryParams, SMachineContext c) {
        
        threadID = tid;
    
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
    
SMachineContextRef TCB::getMachineContext() {
    return &context;
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