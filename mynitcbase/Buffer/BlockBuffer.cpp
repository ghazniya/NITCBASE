#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);
    return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    int blockNum = this->blockNum;
    int currentSlotNum = slotNum;
    
    
    while (blockNum != -1) {
        struct HeadInfo head;
        /*unsigned char buffer[BLOCK_SIZE];
        Disk::readBlock(buffer, blockNum);*/ // direct from disk
        RecBuffer currentblock(blockNum);
        unsigned char *bufferPtr;
        int ret=currentblock.loadBlockAndGetBufferPtr(&bufferPtr);
        if(ret!=SUCCESS){
            return ret;
        }
        
        memcpy(&head.numSlots, bufferPtr + 24, 4);
        memcpy(&head.numEntries, bufferPtr + 16, 4);
        memcpy(&head.numAttrs, bufferPtr + 20, 4);
        memcpy(&head.rblock, bufferPtr + 12, 4);
        
        int attrCount = head.numAttrs;
        int slotCount = head.numSlots;
        int recordSize = attrCount * ATTR_SIZE;
        
        if (currentSlotNum < slotCount) {
            int offset = HEADER_SIZE + slotCount + (recordSize * currentSlotNum);
            unsigned char *slotPointer = bufferPtr + offset;
            memcpy(rec, slotPointer, recordSize);
            return SUCCESS;
        }
        
        currentSlotNum -= slotCount;  
        blockNum = head.rblock;
        
        if (blockNum == -1) {
            return E_OUTOFBOUND;
        }
    }
    
    return E_OUTOFBOUND;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
    int bufferNum=StaticBuffer::getBufferNum(this->blockNum);
    if(bufferNum == E_BLOCKNOTINBUFFER){
        bufferNum=StaticBuffer::getFreeBuffer(this->blockNum);
        if(bufferNum==E_OUTOFBOUND){
            return E_OUTOFBOUND;
        }
        Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);
    }
    *buffPtr=StaticBuffer::blocks[bufferNum];
    return SUCCESS;

}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }
  struct HeadInfo head;
  this->getHeader(&head);
  int slotCount = head.numSlots;
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;
  memcpy(slotMap,slotMapInBuffer,slotCount);
  return SUCCESS;
}
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;
    if (attrType == STRING)
       diff = strcmp(attr1.sVal, attr2.sVal);

    else
        diff = attr1.nVal - attr2.nVal;

    if (diff > 0)  return 1;
    if (diff < 0)  return -1;
     return 0;

}