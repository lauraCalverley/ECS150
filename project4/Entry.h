#ifndef ENTRY_HEADER
#define ENTRY_HEADER

#include "VirtualMachine.h"
#include <vector>
using namespace std;

class Entry {
    
public:
    struct Sector{
        int sectorNumber;
//        char *data;
        char data[512]; // FIXME FIXME FIXME
    };

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
