#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer(){
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        metainfo[bufferIndex].free=true;
    }
}
StaticBuffer::~StaticBuffer(){}

int StaticBuffer::getFreeBuffer(int blockNum){
    if(blockNum<0 || blockNum>DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    int allocatedBuffer=-1;
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free){
            allocatedBuffer=bufferIndex;
            break;
        }
    }
    if(allocatedBuffer==-1){
        return E_CACHEFULL;
    }
    metainfo[allocatedBuffer].free=false;
    metainfo[allocatedBuffer].blockNum=blockNum;
    return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum){
    if(blockNum<0 || blockNum>DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].blockNum==blockNum){
            return bufferIndex;
        }
    }
    return E_BLOCKNOTINBUFFER;
}