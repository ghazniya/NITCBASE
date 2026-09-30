#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getBlockNum() {
    return this->blockNum;
}

/*
 * Brings the block into the buffer if it isn't already there and hands back a
 * pointer to it. A block that was already buffered becomes the most recently
 * used one, so its timestamp is reset while every other occupied buffer ages.
 */
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
    int bufferNum=StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum!=E_BLOCKNOTINBUFFER){
        if(bufferNum==E_OUTOFBOUND){
            return E_OUTOFBOUND;               // E_OUTOFBOUND
        }
        for(int i=0;i<BUFFER_CAPACITY;i++){
            if(StaticBuffer::metainfo[i].free==false){
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }
        StaticBuffer::metainfo[bufferNum].timeStamp=0;
    }else{
        bufferNum=StaticBuffer::getFreeBuffer(this->blockNum);
        if(bufferNum==E_OUTOFBOUND){
            return E_OUTOFBOUND;               // E_OUTOFBOUND / E_CACHEFULL
        }
        Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);
    }

    *buffPtr=StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    memcpy(&head->blockType,  bufferPtr,      4);
    memcpy(&head->pblock,     bufferPtr + 4,  4);
    memcpy(&head->lblock,     bufferPtr + 8,  4);
    memcpy(&head->rblock,     bufferPtr + 12, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs,   bufferPtr + 20, 4);
    memcpy(&head->numSlots,   bufferPtr + 24, 4);
    return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;
    int ret=this->getHeader(&head);
    if(ret!=SUCCESS){
        return ret;
    }

    int attrCount=head.numAttrs;
    int slotCount=head.numSlots;

    if(slotNum<0 || slotNum>=slotCount){
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    int recordSize=attrCount*ATTR_SIZE;
    unsigned char *slotPointer=bufferPtr + HEADER_SIZE + slotCount + (recordSize*slotNum);
    memcpy(rec,slotPointer,recordSize);

    return SUCCESS;
}

/*
 * Overwrites slot slotNum of this block with the given record and marks the
 * buffer dirty, so the change reaches disk on eviction or on shutdown.
 */
int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);
    int attrCount=head.numAttrs;
    int slotCount=head.numSlots;

    if(slotNum<0 || slotNum>=slotCount){
        return E_OUTOFBOUND;
    }


    int recordSize=attrCount*ATTR_SIZE;
    unsigned char *slotPointer=bufferPtr + HEADER_SIZE + slotCount + (recordSize*slotNum);
    memcpy(slotPointer,rec,recordSize);

    StaticBuffer::setDirtyBit(this->blockNum);

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
