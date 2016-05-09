#ifndef MEMORY_POOL_HEADER
#define MEMORY_POOL_HEADER

#include <list>
#include "VirtualMachine.h"

struct MemCell {
    char *cellStart;
    TVMMemorySize cellSize;
};


class MemoryPool {
    TVMMemoryPoolID ID;
    TVMMemorySize size;
    char *start;
    std::list<MemCell*> freeList; // FIXME - use new
    std::list<MemCell*> allocatedList; // FIXME - use new
    bool deleted;
    
public:
    MemoryPool(void *base, TVMMemorySize memSize, TVMMemoryPoolIDRef memoryID);
    TVMMemoryPoolID getMemoryPoolID();
    void setMemoryPoolID(TVMMemoryPoolID id);
    TVMMemorySize bytesLeft();
    int getDeleted();
    void setDeleted(int i=1);

    char* allocate(TVMMemorySize roundedSize);
    char* deallocate(char *pointer);

};


#endif