#ifndef MEMORY_POOL_HEADER
#define MEMORY_POOL_HEADER

struct MemCell {
    char *cellStart;
    TVMMemorySize cellSize;
    
    MemCell(char* start, TVMMemorySize size) {
        cellStart = start;
        cellSize = size;
    }
};


class MemoryPool {
    TVMMemoryPoolID ID;
    TVMMemorySize size;
    char *start;
    list<MemCell> freeList; // FIXME - use new
    
public:
    MemoryPool(void *base, TVMMemorySize memSize, TVMMemoryPoolIDRef memoryID) {
        start = (char*)base;
        size = memSize;
        ID = *memoryID;
        
        MemCell* cell = new MemCell(start, size);
        // push cell to freeList
        
    }
    
    
};


#endif