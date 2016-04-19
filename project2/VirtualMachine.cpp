#include <stdlib.h>
#include <unistd.h>
#include <iostream> // temp?

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {
    using namespace std;
    
    volatile int SLEEPCOUNT = 0; // eventually need a global queue of TCB's or thread SleepCount values

    TVMMainEntry VMLoadModule(const char *module);
    void callbackMachineRequestAlarm(void *calldata);
    void callbackMachineFileOpen(void *calldata, int result);
    
    // The following are defined in VirtualMachineUtils.c:
    // TVMMainEntry VMLoadModule(const char *module)
    // void VMUnloadModule(void)
    // TVMStatus VMFilePrint(int filedescriptor, const char *format, ...)

    
    TVMStatus VMStart(int tickms, int argc, char *argv[]) {
        MachineInitialize();
        
        MachineRequestAlarm(100*1000, callbackMachineRequestAlarm, NULL); // 2nd arg is a function pointer

        TVMMainEntry module = VMLoadModule(argv[0]);
        if (module == NULL) {
            return VM_STATUS_FAILURE; // FIXME doesn't seem to match Nitta's error message
        }
        else {
            module(argc, argv);
            return VM_STATUS_SUCCESS;
        }
    }
    
    void callbackMachineRequestAlarm(void *calldata) {
        SLEEPCOUNT--;
    }
    
    
    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

        if ((data==NULL) || (length==NULL)) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        int writeStatus;
        writeStatus = write(filedescriptor, data, *length); // FIXME - temporary solution // eventually use MachineFileWrite
        if (writeStatus < 0) {
            return VM_STATUS_FAILURE;
        }
        else {
            return VM_STATUS_SUCCESS;
        }
    }
    
    TVMStatus VMThreadSleep(TVMTick tick) {
        
        if (tick == VM_TIMEOUT_INFINITE) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        else if (tick == VM_TIMEOUT_IMMEDIATE) {
            // FIXME? No idea if this is right
            // If tick is specified as VM_TIMEOUT_IMMEDIATE the current process yields the remainder of its processing quantum to the next ready process of equal priority.
            return VM_STATUS_SUCCESS;
        }
        else {
            SLEEPCOUNT = tick; // set global
            while (SLEEPCOUNT != 0) {}
            return VM_STATUS_SUCCESS;
        }
    }
    
    
    
    // VMFileOpen
    
    TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor) {
        
        // VMFileOpen() attempts to open the file specified by filename, using the flags specified by flags parameter, and mode specified by mode parameter. The file descriptor of the newly opened file will be placed in the location specified by filedescriptor. The flags and mode values follow the same format as that of open system call. The filedescriptor returned can be used in subsequent calls to VMFileClose(), VMFileRead(), VMFileWrite(), and VMFileSeek(). When a thread calls VMFileOpen() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful opening of the file is completed.
        
        // Upon successful opening of the file, VMFileOpen() returns VM_STATUS_SUCCESS, upon failure VMFileOpen() returns VM_STATUS_FAILURE.
        
        if ((filename==NULL) || (filedescriptor==NULL)) {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        
        cout << "filedescriptor BEFORE" << *filedescriptor << endl;
        MachineFileOpen(filename, flags, mode, callbackMachineFileOpen, filedescriptor); // FIXME - the parameters for call back are wrong...callbackMachineFileOpen isn't ever called
        cout << "filedescriptor AFTER" << *filedescriptor << endl;
        
        // When a thread calls VMFileOpen() it blocks in the wait state VM_THREAD_STATE_WAITING until the either successful or unsuccessful opening of the file is completed.

        
    }
    
    void callbackMachineFileOpen(void *calldata, int result) {
        // result is the File Descriptor, which is the result of MachineFileOpen
     
        cout << "in callback" << endl;
        cout << "result/FD is " << result << endl;
        *((int*)calldata) = result; // SOURCE: http://stackoverflow.com/questions/1327579/if-i-have-a-void-pointer-how-do-i-put-an-int-into-it
        
        // The file descriptor of the newly opened file will be placed in the location specified by filedescriptor
    }
    
/* temp changes to sleep.c
#include "VirtualMachine.h"
#include <fcntl.h>
    
    void VMMain(int argc, char *argv[]){
        int FileDescriptor;
        
        VMPrint("Going to sleep for 10 ticks\n");
        //VMThreadSleep(10);
        VMFileOpen("test.txt", O_CREAT | O_TRUNC | O_RDWR, 0644, &FileDescriptor);
        VMPrint("Awake\nGoodbye\n");
    }

 */

    
    // VMFileSeek
    // VMFileRead
    // VM FileClose
    
    
    
    
}



/*
 Helpful things:
 ps aux | grep vm
 kill -9 <process_id>
 */

/*
 Pseudocode for Scheduling Algorithm
 
 
*/