#include <list>
#include "MemoryPool.h"

using namespace std;

MemoryPool::MemoryPool(void *base, TVMMemorySize memSize, TVMMemoryPoolIDRef memoryID) {
    start = (char*)base;
    size = memSize;
    ID = *memoryID;
    
    MemCell* cell = new MemCell;
    cell->cellStart = start;
    cell->cellSize = size;
    freeList.push_back(cell);
}


TVMMemoryPoolID MemoryPool::getMemoryPoolID() {
    return ID;
}
void MemoryPool::setMemoryPoolID(TVMMemoryPoolID id) {
    ID = id;
}


/*char* MemoryPool::find(TVMMemorySize size) {
    // rounding up to the nearest 64 bytes will have taken place in VirtualMachine.cpp
    
    for (list<MemCell*>::iterator it=freeList.begin(); it != freeList.end(); it++) {
        if (it->cellSize >= size) {
            return it->cellStart;
        }
    }
    
    return NULL;
}

char* MemoryPool::allocate(TVMMemorySize size) {
    char* memoryLocation;
    memoryLocation = find(size);
    
    if
    
}

void MemoryPool::deallocate() {
    
}*/

TVMMemorySize MemoryPool::bytesLeft() {
    TVMMemorySize total = 0;
    for (list<MemCell*>::iterator it=freeList.begin(); it != freeList.end(); it++) {
        total += (*it)->cellSize;
    }
    return total;
}


int MemoryPool::getDeleted() {
    return deleted;
}
void MemoryPool::setDeleted(int i) {
    deleted = i;
}





