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

int BlockAccess::renameRelation(char oldName[ATTR_SIZE],char newName[ATTR_SIZE]){
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;
    strcpy(newRelationName.sVal,newName);
    char RelcatattrName[]=RELCAT_ATTR_RELNAME;
    RecId recId=linearSearch(RELCAT_RELID,RelcatattrName,newRelationName,EQ);
    if(recId.block!=-1 && recId.slot!=-1){
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName;
    strcpy(oldRelationName.sVal,oldName);
    recId=linearSearch(RELCAT_RELID,RelcatattrName,oldRelationName,EQ);
    if(recId.block==-1 && recId.slot==-1){
        return E_RELNOTEXIST;
    }
    RecBuffer relCatBlock(recId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord,recId.slot);
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,newName);
    relCatBlock.setRecord(relCatRecord,recId.slot);

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numAttrs=relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    char attrcatAttrName[]=ATTRCAT_ATTR_RELNAME;
    for(int i=0;i<numAttrs;i++){
        RecId attrRecId=linearSearch(ATTRCAT_RELID,attrcatAttrName,oldRelationName,EQ);
        RecBuffer attrCatBlock(attrRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord,attrRecId.slot);
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,newName);
        attrCatBlock.setRecord(attrCatRecord,attrRecId.slot);
    }
    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE],char oldName[ATTR_SIZE],char newname[ATTR_SIZE]){
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    
    //searching the relation is there in relation catalog
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal,relName);
    char relcatAttrName[]=RELCAT_ATTR_RELNAME;
    RecId relRecId=linearSearch(RELCAT_RELID,relcatAttrName,relNameAttr,EQ);
    if(relRecId.block==-1 && relRecId.slot==-1)
        return E_RELNOTEXIST;

    
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    RecId atttrtorenameRecId={-1,-1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
    char attrcatAttrName[]=ATTRCAT_ATTR_RELNAME;
    //checks in attribute catalog whether old name exist and updates attribute attrtorenameRecId
    while(true){
        RecId recId=linearSearch(ATTRCAT_RELID,attrcatAttrName,relNameAttr,EQ);
        if(recId.block==-1 && recId.slot==-1){
            break;
        }
        RecBuffer attrCatBlock(recId.block);
        attrCatBlock.getRecord(attrCatEntryRecord,recId.slot);
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,oldName)==0){
            atttrtorenameRecId=recId;
        }
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newname)==0){
            return E_ATTREXIST;
        }
    }
    if(atttrtorenameRecId.block==-1 && atttrtorenameRecId.slot==-1){
        return E_ATTRNOTEXIST;
    }
    //changes the name to newname
    RecBuffer attrCatBlock(atttrtorenameRecId.block);
    attrCatBlock.getRecord(attrCatEntryRecord,atttrtorenameRecId.slot);
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newname);
    attrCatBlock.setRecord(attrCatEntryRecord,atttrtorenameRecId.slot);

    return SUCCESS;
}
