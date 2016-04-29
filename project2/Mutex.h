#ifndef MUTEX_HEADER
#define MUTEX_HEADER

#include <queue>
#include "TCB.h"
#include "VirtualMachine.h"

using namespace std;

class Mutex {
public:
    TVMMutexID id;
    bool value;
    //TCB* owner;
    TVMThreadID owner;
    priority_queue<TCB> waiting;
    
    bool deleted;
    
    Mutex(TVMMutexID mutexID);
};


#endif