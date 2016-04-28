#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"
#include "TCB.h"
#include <vector>
#include <queue>

extern "C" {
using namespace std;

#define VM_THREAD_PRIORITY_IDLE                  ((TVMThreadPriority)0x00)

TVMThreadID CURRENT_THREAD = 0;
vector<TCB*> threadVector;
priority_queue<TCB*> readyQueue, waitingQueue;

//volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values
int TICKMS;
volatile int TICK_COUNT = 0;
volatile int MACHINE_FILE_OPEN_STATUS = 0;
volatile int MACHINE_FILE_SEEK_STATUS = 0;
volatile int MACHINE_FILE_READ_STATUS = 0;
volatile int MACHINE_FILE_WRITE_STATUS = 0;
volatile int MACHINE_FILE_CLOSE_STATUS = 0;

TVMMainEntry VMLoadModule(const char *module);

void idle(void* x);

void callbackMachineRequestAlarm(void *calldata);
void callbackMachineFileOpen(void *calldata, int result);
void callbackMachineFileSeek(void *calldata, int result);
void callbackMachineFileRead(void *calldata, int result);
void callbackMachineFileWrite(void *calldata, int result);
void callbackMachineFileClose(void *calldata, int result);

bool threadExists(TVMThreadID thread);
void entrySkeleton(void *thread);
void Scheduler(int transition, TVMThreadID thread);

// The following are defined in VirtualMachineUtils.c:
// TVMMainEntry VMLoadModule(const char *module)
// void VMUnloadModule(void)
// TVMStatus VMFilePrint(int filedescriptor, const char *format, ...)

TVMStatus VMTickMS(int *tickmsref) {
    if (tickmsref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickmsref = TICKMS;
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMTickCount(TVMTickRef tickref) {
    if (tickref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        *tickref = TICK_COUNT;
        return VM_STATUS_SUCCESS;
    }
}
    
    
TVMStatus VMStart(int tickms, int argc, char *argv[]) {
    TICKMS = tickms;
    MachineInitialize();
    MachineRequestAlarm(tickms*1000, callbackMachineRequestAlarm, NULL); // 2nd arg is a function pointer
    TVMMainEntry module = VMLoadModule(argv[0]);
    if (module == NULL) {
        return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
    }
    else {
        // create main TCB
        SMachineContext mcntxMain; // placeholder: this will be assigned when context is switched
        TVMThreadID mainTID = threadVector.size();
        TCB* mainThread = new TCB(mainTID, NULL, 0, VM_THREAD_STATE_RUNNING, VM_THREAD_PRIORITY_NORMAL, NULL, NULL, mcntxMain);
        threadVector.push_back(mainThread);

        // create idle thread and TCB; activate idle thread
        TVMThreadID idleTID;
		VMThreadCreate(idle, NULL, 0x10000, VM_THREAD_PRIORITY_IDLE, &idleTID); // pushed back in VMThreadCreate
        VMThreadActivate(idleTID);
        
        module(argc, argv);
        
        /*CURRENT_THREAD = 1;
        MachineContextSwitch(threadVector[mainTID]->getMachineContext(),threadVector[idleTID]->getMachineContext());
        CURRENT_THREAD = 0;
        //cout << "in between" << endl;
        MachineContextSwitch (threadVector[mainTID]->getMachineContext(),threadVector[idleTID]->getMachineContext());
        module(argc, argv); */
        
        // FIXME deallocate memory for TCBs and such
        
        
        return VM_STATUS_SUCCESS;
    }
}

void idle(void* x)  {
	while (true) {
        cout << "in idle" << endl;
    }
    //cout << "in idle" << endl;
}

void callbackMachineRequestAlarm(void *calldata) {
    TICK_COUNT++;
    
    for (int i=0; i < threadVector.size(); i++) { // IMPROVEMENT sleepVector idea
        if ((threadVector[i]->getDeleted() == 0) && (threadVector[i]->getSleepCount() > 0)) {
            threadVector[i]->decrementSleepCount();
        }
    }
    
}

    
TVMStatus VMThreadSleep(TVMTick tick) {
    if (tick == VM_TIMEOUT_INFINITE) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else if (tick == VM_TIMEOUT_IMMEDIATE) {
        threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_READY);
        readyQueue.push(threadVector[CURRENT_THREAD]);
        //Scheduler()
        return VM_STATUS_SUCCESS;
    }
    else {
        threadVector[CURRENT_THREAD]->setSleepCount(tick);
        threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_WAITING);
        waitingQueue.push(threadVector[CURRENT_THREAD]);

        while (threadVector[CURRENT_THREAD]->getSleepCount() != 0) {}
        // call Scheduler(); // ?
        return VM_STATUS_SUCCESS;
    }
}


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

    if ((data==NULL) || (length==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileWrite(filedescriptor, data, *length, callbackMachineFileWrite, length);
    
    while (MACHINE_FILE_WRITE_STATUS != 1) {} // FIXME Multi When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
    
    MACHINE_FILE_WRITE_STATUS = 0; // reset
    
    if (*length < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileWrite(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_WRITE_STATUS = 1;
}





TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
    
    if ((filename==NULL) || (filedescriptor==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileOpen(filename, flags, mode, callbackMachineFileOpen, filedescriptor);
    // save the CURRENT_THREAD id
    // while (threadVector[savedCURRENTTHREAD]->getState()==WAITING)
    while (MACHINE_FILE_OPEN_STATUS != 1) {} // FIXME Multi When a thread calls VMFileOpen() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful opening of the file is completed.
    MACHINE_FILE_OPEN_STATUS = 0; // reset

    if (*filedescriptor < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileOpen(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_OPEN_STATUS = 1;
}


TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset) {
    
    MachineFileSeek(filedescriptor, offset, whence, callbackMachineFileSeek, newoffset);
    
    while (MACHINE_FILE_SEEK_STATUS != 1) {} // FIXME Multi When a thread calls VMFileSeek() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful seeking in the file is completed.
    MACHINE_FILE_SEEK_STATUS = 0; // reset
    
    if (*newoffset < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileSeek(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_SEEK_STATUS = 1;
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length) {
    if ((data==NULL) || (length==NULL)) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    
    MachineFileRead(filedescriptor, data, *length, callbackMachineFileRead, length);
    
    while (MACHINE_FILE_READ_STATUS != 1) {} // FIXME Multi When a thread calls VMFileRead() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful reading of the file is completed.
    MACHINE_FILE_READ_STATUS = 0; // reset
    
    if (*length < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileRead(void *calldata, int result) {
    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_READ_STATUS = 1;
}

TVMStatus VMFileClose(int filedescriptor) {
    int status = -1; // 0 is success, < 0 is failure
    MachineFileClose(filedescriptor, callbackMachineFileClose, &status);

    while (MACHINE_FILE_CLOSE_STATUS != 1) {} // FIXME Multi When a thread calls VMFileClose() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful closing of the file is completed.

    MACHINE_FILE_CLOSE_STATUS = 0; // reset
    
    //if (*status < 0) {
    if (status < 0) {
        return VM_STATUS_FAILURE;
    }
    else {
        return VM_STATUS_SUCCESS;
    }
}

void callbackMachineFileClose(void *calldata, int result) {

    *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
    MACHINE_FILE_CLOSE_STATUS = 1;
}

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
    
    /*if ((entry==NULL) || (tid==NULL)) {
        cout << "";
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }*/
    if (entry==NULL) {
        cout << ""; //FIMXE cout issue - w/o, it seg faults // NITTA says there is a bigger issue with our stack and such
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    if (tid==NULL) {
        cout << ""; //FIMXE cout issuecle - w/o, it seg faults
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }

    //cout << "not in if" << endl;
	char *stackPointer = new char[memsize];
	SMachineContext mcntx;
    
    TVMThreadID newThreadID = threadVector.size();
    TCB* thread = new TCB(newThreadID, stackPointer, memsize, VM_THREAD_STATE_DEAD, prio, entry, param, mcntx);
    threadVector.push_back(thread);
    //tid = threadVector[newThreadID]->getThreadIDRef(); // THIS IS THE BAD WAY
    *tid = threadVector[newThreadID]->getThreadID(); // (OPTIONAL) FIXME: if id was later updated, don't think variable external to this function would be made aware of the change...
	
	return VM_STATUS_SUCCESS;
    
}

TVMStatus VMThreadID(TVMThreadIDRef threadref) {
	if (threadref == NULL) {
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else {
        //cout << "";
        //cout << "CURRENT_THREAD IS " << CURRENT_THREAD << endl;
        *threadref = threadVector[CURRENT_THREAD]->getThreadID();
        
        //TVMThreadID temp = threadVector[CURRENT_THREAD]->getThreadID();
        //threadref = &temp;
        //cout << "value of threadref is" << *threadref << endl;
		return VM_STATUS_SUCCESS;
	}
}

TVMStatus VMThreadDelete(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setDeleted();
        return VM_STATUS_SUCCESS;
    }
}
    
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (stateref == NULL) {
        return VM_STATUS_ERROR_INVALID_PARAMETER;
    }
    else {
        //TVMThreadState state = threadVector[thread]->getTVMThreadState();
        *stateref = threadVector[thread]->getTVMThreadState();
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadActivate(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() != VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        MachineContextCreate(threadVector[thread]->getMachineContext(), entrySkeleton, threadVector[thread], threadVector[thread]->getStackPointer(), threadVector[thread]->getStackSize());
        Scheduler(5, thread); //readyQueue.push(threadVector[thread]); //threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY); // FIXME ordering??
        return VM_STATUS_SUCCESS;
    }
}

TVMStatus VMThreadTerminate(TVMThreadID thread) {
    if (!threadExists(thread)) {
        return VM_STATUS_ERROR_INVALID_ID;
    }
    else if (threadVector[thread]->getTVMThreadState() == VM_THREAD_STATE_DEAD) {
        return VM_STATUS_ERROR_INVALID_STATE;
    }
    else {
        threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_DEAD);
        CURRENT_THREAD = 0; // FIXME Scheduler()
        MachineContextSwitch(threadVector[1]->getMachineContext(),threadVector[0]->getMachineContext()); // FIXME Scheduler()
        //cout << "thread was terminated" << endl;
        // Thread will still be in the ready or waiting queue, so we need to check the state in Scheduler()
        // FIXME and must release any mutexes that it currently holds.
        // FIXME??? Scheduling: The termination of a thread can trigger another thread to be scheduled.
        return VM_STATUS_SUCCESS;
    }
}
    
bool threadExists(TVMThreadID thread) {
    bool exists;
    if (thread >= threadVector.size())
    {
        exists = 0;
    }
    else if (threadVector[thread]->getDeleted() == 1) {
        exists = 0;
    }
    else {
        exists = 1;
    }
    return exists;
}
    
/*
TVMStatus VMThreadSleep(TVMTick tick);

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref);
TVMStatus VMMutexDelete(TVMMutexID mutex);
TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref);
TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout);
TVMStatus VMMutexRelease(TVMMutexID mutex);
*/
    
//void entrySkeleton(TVMThreadEntry entry, void *params) { // FIXME???
void entrySkeleton(void *thread) { // FIXME???
    TCB* theThread = (TCB*)thread;
    TVMThreadEntry entry = theThread->getTVMThreadEntry();
    void* entryParams = theThread->getParams();
    entry(entryParams);
	VMThreadTerminate(CURRENT_THREAD);
}


void Scheduler(int transition, TVMThreadID thread) {

    switch (transition) {
        case 1: { // I/O, acquire mutex or timeout
            // assume callbacks, etc. receive the TCB/TCB* of the thread that called the function associated with the callback
            // pop waitingQueue??? OR pop mutexQueue          // have a sleepVector, mutexQueue
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(threadVector[thread]);

            if (threadVector[thread]->getTVMThreadPriority() > threadVector[CURRENT_THREAD]->getTVMThreadPriority()) {
                // make thread the running thread MCS
                Scheduler(2, thread);
                //threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_READY);
            }
            break;
        }
        case 2: { // Scheduler selects process // Scheduler (2, CURRENT_THREAD)
            TVMThreadID readyThread = readyQueue.top()->getThreadID();
            threadVector[readyThread]->setTVMThreadState(VM_THREAD_STATE_RUNNING);
            readyQueue.pop(); // DEBUG potentially
            CURRENT_THREAD = readyThread;
            MachineContextSwitch(threadVector[thread]->getMachineContext(),threadVector[readyThread]->getMachineContext());
            break;
        }
        case 3: { // Process quantum up // only ever called by Scheduler(), in alarm callback, call Scheduler(2, NULL)
            // thread parameter is the thread that is about to start running
            //  Each thread gets one quantum of time (one tick). They are then put in the ready state and scheduling is done, this means that the scheduler will need to be called in the alarm callback.
            threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(threadVector[CURRENT_THREAD]);
            Scheduler(2, CURRENT_THREAD);
            break;
        }
        case 4: { // Process terminates
            TVMThreadState oldThreadState = threadVector[thread]->getTVMThreadState();
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_DEAD);

            switch (oldThreadState) {
                case VM_THREAD_STATE_RUNNING: {
                    Scheduler(2, CURRENT_THREAD);
                    break;
                }
                case VM_THREAD_STATE_READY: {
                    Vector<TCB*> tempReadyVector;
                    
                    // remove thread from readyQueue
                    while (!readyQueue.empty()) {
                        TCB *top = readyQueue.top();
                        if (top->getThreadID() != thread->getThreadID()) {
                            tempReadyVector.push_back(top);
                        }
                        readyQueue.pop();
                    }
                    for (int i=0; i < tempReadyVector.size(); i++) {
                        readyQueue.push(tempReadyVector[i]);
                    }
                    break;
                }
                case VM_THREAD_STATE_WAITING: {
                    
                    break;
                }
                    
                default:
                    break;
            }
            
            
            threadVector[CURRENT_THREAD]->setTVMThreadState(VM_THREAD_STATE_DEAD);

            
            break;
        }
        case 5: { // Process activated
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_READY);
            readyQueue.push(threadVector[thread]);
            
            //compare running's priority to thread's priority; if thread > running: switch context and put running into ready queue, else: nothing
            break;
        }
        case 6: { // Process blocks // Scheduler(6. CURRENT_THREAD)
            // move the next 2 lines to the functions that cause something to go to waiting (mutex, file functions, VMThreadSleep)
            threadVector[thread]->setTVMThreadState(VM_THREAD_STATE_WAITING);
            // put on waiting queue
            
            
            Scheduler(2, CURRENT_THREAD);

            break;
        }
            
        default:
            break;
    }
    
    // MCS occurs when you swap the Running threads
    
    // 3 4 and 6 all call 2
    
    // in VMThreadTerminate
    // Thread will still be in the ready or waiting queue, so we need to check the state in Scheduler()

    // 6 is waiting to acquire mutex successfully, waiting for I/O (i.e. a callback from one of the VMFile functions, or sleeping
    // 1 is acquired mutex, got a/in a callback saying I/O was done, or no longer sleeping (SLEEPCOUNT for the thread = 0)
    
    // VMFile callback functions, VMThreadSleep, Mutex function callbacks(presumably) will call Scheduler(1);
    // VMFile functions will call Scheduler(6); then Scheduler(2)
    // VMThreadTerminate will call Scheduler(4); if terminated thread was RUNNING, call Scheduler(2);
    //* VMThreadActivate will call Scheduler (5)
    // MachineAlarmCallback will call Scheduler(3) and then Scheduler(2)
    
    
    // Scheduler() will figure out what the next thread/context is (readyQueue top)
    //change oldState
    //change newState
    //MCS()
    
/*
 Pseudocode for Scheduling Algorithm
 
 So you call the machine function, your current thread blocks, and you switch to the next ready thread.
 * Once this other thread gets your callback, you will put the thread that was blocking in the ready state, and then check if its priority is higher than the current running thread.  if it is, then you switch threads. if its not then it stays in the ready state.
 
*/


}
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 *
 *  // @567 @630 @676 @716
 */


/* To Do List
 Scheduler()
 redo all VMFile* - call Scheduler(transition, threadthatisnolongerwaiting), need to pass the thread that is being worked on in the callback data, need to add a "waiting for callback" data member to the TCB class
 -- combine all the File callbacks into one
 -- in callback, save CURRENT_THREAD (because WILL change elsewhere while waiting)
 -- while (savedCurrentThread == WAITING)
 
 -- in callback, set the thread it's passed (in callback data) to READY instead of WAITING
 
 
 
 VMMutexCreate
 VMMutexDelete
 VMMutexQuery
 VMMutexAcquire
 VMMutexRelease
 */
