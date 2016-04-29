#include <queue>
#include "VirtualMachine.h"
#include "TCB.h"
#include "Mutex.h"

using namespace std;

Mutex::Mutex(TVMMutexID mutexID) {
    id = mutexID;
    value = 1;
    //ownerID
    //owner = new TCB();
    //owner = NULL;
    deleted = 0;
    //waiting = new priority_queue<TCB>;
}
