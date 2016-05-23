#include "BPB.h"
#include <stdint.h>

BPB::BPB(uint8_t BPB_SecPerClus, uint16_t BPB_RsvdSecCnt, uint8_t BPB_NumFATs, uint16_t BPB_RootEntCnt, uint16_t BPB_FATSz16, uint32_t BPB_TotSec32) :
BPB_SecPerClus (BPB_SecPerClus),
BPB_RsvdSecCnt (BPB_RsvdSecCnt),
BPB_NumFATs (BPB_NumFATs),
BPB_RootEntCnt (BPB_RootEntCnt),
BPB_FATSz16 (BPB_FATSz16),
BPB_TotSec32 (BPB_TotSec32)


{
    FirstRootSector = BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16;
    RootDirectorySectors = (BPB_RootEntCnt * 32) / 512;
    FirstDataSector = FirstRootSector + RootDirectorySectors;
    ClusterCount = (BPB_TotSec32 - FirstDataSector) / BPB_SecPerClus;

}