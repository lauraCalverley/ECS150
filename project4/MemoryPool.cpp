#include <list>
#include <iterator>
#include "MemoryPool.h"

#include <iostream> // temp

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
        if ((*it)->cellSize >= roundedSize) {

            //creates cell for allocatedList
            MemCell* cell = new MemCell;
            cell->cellStart = (*it)->cellStart;
            cell->cellSize = roundedSize;
            allocatedList.push_back(cell);
            
            // check for other cases: entire free cell is allocated
            char *locationStart = (*it)->cellStart;
            if ((*it)->cellSize != roundedSize) {
                //adjusts cell in freeList
                (*it)->cellStart += roundedSize;
                (*it)->cellSize -= roundedSize;
            }
            else {
                freeList.erase(it);
            }
            
            return locationStart;
        }
    }
    
    return NULL;
}

char* MemoryPool::deallocate(char *pointer) {
    //cout << "in deallocate" << endl;
    //cout << "allocatedList.size()" << allocatedList.size() << endl;
    //cout << "freeList.size()" << freeList.size() << endl;
    
    for (list<MemCell*>::iterator it=allocatedList.begin(); it != allocatedList.end(); it++) {
        //cout << "in OUTER FOR" << endl;
        if ((*it)->cellStart == pointer) { // 'it' is the cell to deallocate // found the cell in the allocatedList
            //cout << "in BIG IF" << endl;
            list<MemCell*>::iterator target = it;
            
            if (freeList.size() == 0) {
                //cout << "A" << endl;
                freeList.push_front(*target);
            }
            
            if ((pointer < (*freeList.begin())->cellStart) && ((pointer + (*target)->cellSize) == (*freeList.begin())->cellStart)) {
                // cout << "B" << endl;
                // combine with freeList.begin() after pointer's cell
                (*freeList.begin())->cellStart = pointer;
                (*freeList.begin())->cellSize += (*target)->cellSize;
            }
            else if (pointer < (*freeList.begin())->cellStart) {
                //cout << "C" << endl;
                // add new cell for pointer -- push_front/insert
                freeList.push_front(*target);
            }
            

            
            list<MemCell*>::iterator myEnd = freeList.end();
            myEnd--;
            for (list<MemCell*>::iterator freeIt=freeList.begin(); freeIt != myEnd; freeIt++) {
                //cout << "in for" << endl;
                if ((((*freeIt)->cellStart + (*freeIt)->cellSize) <= pointer) && (pointer < ((*freeIt + 1)->cellStart))) {
                    //cout << "D" << endl;
                    list<MemCell*>::iterator left = freeIt;
                    list<MemCell*>::iterator right = freeIt;
                    right++;
                    
                    if ((((*left)->cellStart + (*left)->cellSize) == pointer) && ((*target)->cellStart + (*target)->cellSize == (*right)->cellStart)) {
                        //cout << "E" << endl;
                        // merge all 3
                        (*left)->cellSize += ((*target)->cellSize + (*right)->cellSize);
                        freeList.erase(right);

                    }
                    else if (((*left)->cellStart + (*left)->cellSize) == pointer) {
                        //cout << "F" << endl;
                        // merge leftmost 2
                        (*left)->cellSize += (*target)->cellSize;
                    }
                    else if ((*target)->cellStart + (*target)->cellSize == (*right)->cellStart) {
                        //cout << "G" << endl;
                        // merge rightmost 2
                        (*right)->cellStart = (*target)->cellStart;
                        (*right)->cellSize += (*target)->cellSize;
                    }
                    else {
                        //cout << "H" << endl;
                        freeList.insert(right, *target);
                    }
                }
            }
            
            // after the last cell currently in freeList
            if (((*myEnd)->cellStart) + ((*myEnd)->cellSize) == (*target)->cellStart) {
                //cout << "I" << endl;
                (*myEnd)->cellSize += (*target)->cellSize;
            }
            else if (((*myEnd)->cellStart) + ((*myEnd)->cellSize) < (*target)->cellStart) {
                //cout << "J" << endl;
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



