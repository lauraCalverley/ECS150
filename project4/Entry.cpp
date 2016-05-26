#include "Entry.h"
#include "VirtualMachine.h"

#include <iostream> // temp

Entry::Entry(SVMDirectoryEntry e, int firstClusterNumber, int descriptor) :
    e (e),
    firstClusterNumber (firstClusterNumber),
    descriptor (descriptor)
{
    directory = e.DAttributes & 0x10;
}
