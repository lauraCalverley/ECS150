#ifndef ENTRY_HEADER
#define ENTRY_HEADER

#include "VirtualMachine.h"
#include <string>

class Entry {
    
public:
    SVMDirectoryEntry e;
    int firstClusterNumber;
    int descriptor;
    bool directory;
    
    Entry(SVMDirectoryEntry e, int firstClusterNumber, int descriptor = -1);
};

#endif
