#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer(){
    for(int i=0;i<4;i++){
        Disk::readBlock(&blockAllocMap[i*BLOCK_SIZE],i);
    }
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        metainfo[bufferIndex].free=true;
        metainfo[bufferIndex].dirty=false;
        metainfo[bufferIndex].blockNum=-1;
        metainfo[bufferIndex].timeStamp=-1;
    }
}

// write back every dirty block that is still held in the buffer
StaticBuffer::~StaticBuffer(){
    for(int i=0;i<4;i++){
        Disk::writeBlock(&blockAllocMap[i*BLOCK_SIZE],i);
    }
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free==false && metainfo[bufferIndex].dirty==true){
            Disk::writeBlock(blocks[bufferIndex],metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum){
    if(blockNum<0 || blockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }

    // every buffer currently in use gets older by one
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free==false){
            metainfo[bufferIndex].timeStamp++;
        }
    }

    int allocatedBuffer=-1;
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free){
            allocatedBuffer=bufferIndex;
            break;
        }
    }

    if(allocatedBuffer==-1){
        // buffer is full: evict the least recently used block (largest timestamp)
        int largestTimeStamp=-1;
        for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
            if(metainfo[bufferIndex].timeStamp>largestTimeStamp){
                largestTimeStamp=metainfo[bufferIndex].timeStamp;
                allocatedBuffer=bufferIndex;
            }
        }
        if(metainfo[allocatedBuffer].dirty==true){
            Disk::writeBlock(blocks[allocatedBuffer],metainfo[allocatedBuffer].blockNum);
        }
    }

    metainfo[allocatedBuffer].free=false;
    metainfo[allocatedBuffer].dirty=false;
    metainfo[allocatedBuffer].blockNum=blockNum;
    metainfo[allocatedBuffer].timeStamp=0;

    return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum){
    if(blockNum<0 || blockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free==false && metainfo[bufferIndex].blockNum==blockNum){
            return bufferIndex;
        }
    }
    return E_BLOCKNOTINBUFFER;
}


int StaticBuffer::setDirtyBit(int blockNum){
    int buffernum=getBufferNum(blockNum);
    if(buffernum==E_BLOCKNOTINBUFFER){
        return E_BLOCKNOTINBUFFER;
    }
    if(buffernum==E_OUTOFBOUND){
        return E_OUTOFBOUND;
    }
    metainfo[buffernum].dirty=true;
    return SUCCESS;
}