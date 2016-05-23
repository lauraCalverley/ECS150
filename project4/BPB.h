#ifndef BPB_HEADER
#define BPB_HEADER

#include <stdint.h>

class BPB {
        public:
        uint8_t BPB_SecPerClus;
        uint16_t BPB_RsvdSecCnt;
        uint8_t BPB_NumFATs;
        uint16_t BPB_RootEntCnt;
        uint16_t BPB_FATSz16;
        uint32_t BPB_TotSec32;
        
        int FirstRootSector;
        int RootDirectorySectors;
        int FirstDataSector;
        int ClusterCount;
        
        BPB(uint8_t BPB_SecPerClus, uint16_t BPB_RsvdSecCnt, uint8_t BPB_NumFATs,
            uint16_t BPB_RootEntCnt, uint16_t BPB_FATSz16, uint32_t BPB_TotSec32);
};

#endif

