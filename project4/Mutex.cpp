#include <queue>
#include "VirtualMachine.h"
#include "TCB.h"
#include "Mutex.h"

using namespace std;

Mutex::Mutex(TVMMutexID mutexID) {
    id = mutexID;
    value = 1;
    deleted = 0;
}
