#ifndef ENTRY_HEADER
#define ENTRY_HEADER

#include "VirtualMachine.h"

class Entry {
    
public:
    SVMDirectoryEntry e;
    int firstClusterNumber;
    int descriptor;
    bool directory;
    bool writeable;
    int fileOffset;
    vector<Sector> dirtySectors;
    
    Entry(SVMDirectoryEntry e, int firstClusterNumber, int descriptor = -1);
};

#endif
