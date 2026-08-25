#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId,char attrName[ATTR_SIZE],union Attribute attrVal,int op){
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId,&prevRecId);
    int block,slot;
    if(prevRecId.block==-1 && prevRecId.slot==-1){
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId,&relCatEntry);
        block=relCatEntry.firstBlk;
        slot=0;
    }else{
        block=prevRecId.block;
        slot=prevRecId.slot+1;
    }
    while(block!=-1){
        RecBuffer recBuffer(block);
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        if(slot>=head.numSlots){
            block=head.rblock;
            slot=0;
            continue;
        }
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);
        if(slotMap[slot]==SLOT_UNOCCUPIED){
            slot++;
            continue;
        }
        Attribute record[head.numAttrs];
        recBuffer.getRecord(record,slot);
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);
        int offset=attrCatEntry.offset;
        int cmpVal=compareAttrs(record[offset],attrVal,attrCatEntry.attrType);
            if(
                (op==NE && cmpVal!=0)||
                (op==LT && cmpVal<0) ||
                (op==LE && cmpVal<=0) ||
                (op==EQ && cmpVal==0) ||
                (op==GT && cmpVal>0) ||
                (op==GE && cmpVal>=0)
            ){
                RecId recId={block,slot};
                RelCacheTable::setSearchIndex(relId,&recId);
                return recId;
            }
            slot++;
        
    }
    return RecId{-1,-1};
} 
