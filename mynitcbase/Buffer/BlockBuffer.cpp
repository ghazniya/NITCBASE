#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}
BlockBuffer::BlockBuffer(char blockType){
    // map the block-type character to its BlockType enum value so the block
    // header and allocation map store REC/IND_INTERNAL/IND_LEAF, not the raw
    // ASCII code of the character.
    int type;
    if(blockType=='R')      type=REC;
    else if(blockType=='I') type=IND_INTERNAL;
    else if(blockType=='L') type=IND_LEAF;
    else                    type=UNUSED_BLK;
    blockNum=getFreeBlock(type);
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getBlockNum() {
    return this->blockNum;
}

RecBuffer::RecBuffer():BlockBuffer('R'){}

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
int BlockBuffer::setHeader(struct HeadInfo * head){
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    struct HeadInfo *bufferHeader=(struct HeadInfo *)bufferPtr;
    bufferHeader->numSlots=head->numSlots;
    bufferHeader->numAttrs=head->numAttrs;
    bufferHeader->rblock=head->rblock;
    bufferHeader->lblock=head->lblock;
    ret=StaticBuffer::setDirtyBit(blockNum);
    if(ret!=SUCCESS){
        return ret;
    }

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType){
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    *((int32_t *)bufferPtr)=blockType;
    StaticBuffer::blockAllocMap[blockNum]=blockType;
    ret=StaticBuffer::setDirtyBit(blockNum);
    if(ret!=SUCCESS){
        return SUCCESS;
    }
    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){
    int freeblock=-1;
    for(int i=0;i<DISK_BLOCKS;i++){
        if(StaticBuffer::blockAllocMap[i]==UNUSED_BLK){
            freeblock=i;
            break;
        }
    }
    if(freeblock==-1){
        return E_DISKFULL;
    }
    blockNum=freeblock;
    // getFreeBuffer returns the allocated buffer index (>=0), not SUCCESS;
    // only a negative value (E_OUTOFBOUND) is an error. Treating the buffer
    // index as an error here used to make getFreeBlock return that index as
    // the block number, corrupting firstBlk (e.g. pointing at a BMAP block).
    int ret=StaticBuffer::getFreeBuffer(blockNum);
    if(ret<0){
        return ret;
    }

    HeadInfo head;
    head.pblock=-1;
    head.lblock=-1;
    head.rblock=-1;
    head.numAttrs=0;
    head.numEntries=0;
    head.numSlots=0;
    ret=setHeader(&head);
    if(ret!=SUCCESS){
        return ret;
    }

    ret=setBlockType(blockType);
    if(ret!=SUCCESS){
        return ret;
    }

    return blockNum;
}
int RecBuffer::setSlotMap(unsigned char *slotMap){
    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    HeadInfo head;
    ret=getHeader(&head);
    if(ret!=SUCCESS){
        return ret;
    }

    int numSlots=head.numSlots;
    memcpy(bufferPtr+HEADER_SIZE,slotMap,numSlots);
    ret=StaticBuffer::setDirtyBit(blockNum);
    if(ret!=SUCCESS){
        return ret;
    }

    return SUCCESS;

}

void BlockBuffer::releaseBlock(){
    if(this->blockNum==INVALID_BLOCKNUM || this->blockNum<0 || this->blockNum>=DISK_BLOCKS || StaticBuffer::blockAllocMap[this->blockNum]==UNUSED_BLK){
        return;
    }
    int bufferNum=StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum!=E_BLOCKNOTINBUFFER && bufferNum >=0 && bufferNum < BUFFER_CAPACITY){
        StaticBuffer::metainfo[bufferNum].free=true;
    }

    StaticBuffer::blockAllocMap[this->blockNum]=UNUSED_BLK;
    this->blockNum=INVALID_BLOCKNUM;
}