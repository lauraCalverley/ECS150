#include "Entry.h"
#include "VirtualMachine.h"

Entry::Entry(SVMDirectoryEntry e, int firstClusterNumber, int descriptor) :
    e (e),
    firstClusterNumber (firstClusterNumber),
    descriptor (descriptor)
{
    directory = e.DAttributes & 0x10;
    writeable = 0;
    fileOffset = 0;
}

SVMDirectoryEntryRef Entry::getSVMEntryRef(){
	return &e;
}
