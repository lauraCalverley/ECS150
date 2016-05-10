#include <list>
#include <iterator>
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
    for (list<MemCell*>::iterator it=freeList.begin(); it != freeList.end(); it++) {
        if (it->cellSize >= size) {
            return it->cellStart;
        }
    }
    
    return NULL;
}*/

/*char* MemoryPool::allocate(TVMMemorySize size) {
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


char* MemoryPool::allocate(TVMMemorySize roundedSize) {
    for (list<MemCell*>::iterator it=freeList.begin(); it != freeList.end(); it++) {
        if ((*it)->cellSize >= size) {

            //creates cell for allocatedList
            MemCell* cell = new MemCell;
            cell->cellStart = (*it)->cellStart;
            cell->cellSize = roundedSize;
            allocatedList.push_back(cell);
            
            //adjusts cell in freeList
            char *locationStart = (*it)->cellStart;
            (*it)->cellStart += roundedSize;
            (*it)->cellSize -= roundedSize;

            return locationStart;
        }
    }
    
    return NULL;
}

char* MemoryPool::deallocate(char *pointer) {
    for (list<MemCell*>::iterator it=allocatedList.begin(); it != allocatedList.end(); it++) {
        if ((*it)->cellStart == pointer) { // 'it' is the cell to deallocate
            list<MemCell*>::iterator target = it;

            if ((pointer < (*freeList.begin())->cellStart) && (pointer + (*target)->cellSize) == (*freeList.begin())->cellStart) {
                // combine with freeList.begin() after pointer's cell
                (*freeList.begin())->cellStart = pointer;
                (*freeList.begin())->cellSize += (*target)->cellSize;
            }
            else if (pointer < (*freeList.begin())->cellStart) {
                // add new cell for pointer -- push_front/insert
                freeList.push_front(*target);
            }
            
            
            list<MemCell*>::iterator myEnd = freeList.end();
            myEnd--;
            for (list<MemCell*>::iterator freeIt=freeList.begin(); freeIt != myEnd; freeIt++) {
                if ((((*freeIt)->cellStart + (*freeIt)->cellSize) <= pointer) && (pointer < ((*freeIt + 1)->cellStart))) {
                    
                    list<MemCell*>::iterator left = freeIt;
                    list<MemCell*>::iterator right = freeIt;
                    right++;
                    
                    if ((((*left)->cellStart + (*left)->cellSize) == pointer) && ((*target)->cellStart + (*target)->cellSize == (*right)->cellStart)) {
                        // merge all 3
                        (*left)->cellSize += ((*target)->cellSize + (*right)->cellSize);
                        freeList.erase(right);

                    }
                    else if (((*left)->cellStart + (*left)->cellSize) == pointer) {
                        // merge leftmost 2
                        (*left)->cellSize += (*target)->cellSize;
                    }
                    else if ((*target)->cellStart + (*target)->cellSize == (*right)->cellStart) {
                        // merge rightmost 2
                        (*right)->cellStart = (*target)->cellStart;
                        (*right)->cellSize += (*target)->cellSize;
                    }
                    else {
                        freeList.insert(right, *target);
                    }
    
                }
                
            }
            
            // after the last cell currently in freeList
            if (((*myEnd)->cellStart) + ((*myEnd)->cellSize) == (*target)->cellStart) {
                (*myEnd)->cellSize += (*target)->cellSize;
            }
            else if (((*myEnd)->cellStart) + ((*myEnd)->cellSize) < (*target)->cellStart) {
                // create new
                freeList.push_back(*target);
            }
         
            allocatedList.erase(target);
            return pointer;
        }
    }
    
    return NULL;
}


int MemoryPool::getAllocatedListSize() {
    return allocatedList.size();
}



