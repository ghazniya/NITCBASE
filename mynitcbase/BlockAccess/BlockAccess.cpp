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

int BlockAccess::insert(int relId,Attribute *record){

    RelCatEntry relCatEntry;
    int ret=RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(ret!=SUCCESS){
        return ret;
    }

    int blockNum=relCatEntry.firstBlk;
    RecId recid={-1,-1};
    int numOfSlots=relCatEntry.numSlotsPerBlk;
    int numOfAttributes=relCatEntry.numAttrs;
    int prevblock=-1;

    while(blockNum!=-1){
        RecBuffer recBuffer(blockNum);
        HeadInfo header;
        ret=recBuffer.getHeader(&header);
        if(ret!=SUCCESS){
            return ret;
        }

        unsigned char slotMap[numOfSlots];
        ret=recBuffer.getSlotMap(slotMap);
        if(ret!=SUCCESS){
            return ret;
        }

        for(int i=0;i<numOfSlots;i++){
            if(slotMap[i]==SLOT_UNOCCUPIED){
                recid.block=blockNum;
                recid.slot=i;
                break;
            }
        }
        if(recid.block!=-1)
            break;
        prevblock=blockNum;
        blockNum=header.rblock;
    }
    if(recid.block==-1){
        if(relId==RELCAT_RELID){
            return E_MAXRELATIONS;
        }

        RecBuffer newRecBuffer;
        int newBlockNum=newRecBuffer.getBlockNum();
        if(newBlockNum==E_DISKFULL){
            return E_DISKFULL;
        }

        recid.block=newBlockNum;
        recid.slot=0;

         HeadInfo header;

        header.blockType = REC;
        header.pblock = -1;

        if (prevblock == -1)
            header.lblock = -1;
        else
            header.lblock = prevblock;

        header.rblock = -1;
        header.numEntries = 0;
        header.numSlots = numOfSlots;
        header.numAttrs = numOfAttributes;

        ret = newRecBuffer.setHeader(&header);

        if (ret != SUCCESS)
            return ret;
        
        //initialize slot map
        unsigned char slotMap[numOfSlots];

        for (int i = 0; i < numOfSlots; i++)
            slotMap[i] = SLOT_UNOCCUPIED;

        ret = newRecBuffer.setSlotMap(slotMap);

        if (ret != SUCCESS)
            return ret;

        //link new block to the previous last block
        if (prevblock != -1) {

            RecBuffer prevBuffer(prevblock);

            HeadInfo prevHeader;

            ret = prevBuffer.getHeader(&prevHeader);

            if (ret != SUCCESS)
                return ret;

            prevHeader.rblock = recid.block;

            ret = prevBuffer.setHeader(&prevHeader);

            if (ret != SUCCESS)
                return ret;
        }else {

            // This is the first block of the relation
            relCatEntry.firstBlk = recid.block;

            ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);

            if (ret != SUCCESS)
                return ret;
        }
        relCatEntry.lastBlk = recid.block;

        ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);

        if (ret != SUCCESS)
            return ret;
    }
    //nsert record into the selected slot
    RecBuffer recBuffer(recid.block);

    ret = recBuffer.setRecord(record, recid.slot);

    if (ret != SUCCESS)
        return ret;
    
    //Mark slot as occupied
    unsigned char slotMap[numOfSlots];

    ret = recBuffer.getSlotMap(slotMap);

    if (ret != SUCCESS)
        return ret;

    slotMap[recid.slot] = SLOT_OCCUPIED;

    ret = recBuffer.setSlotMap(slotMap);

    if (ret != SUCCESS)
        return ret;
    
    //increment noofentries in block
    HeadInfo header;

    ret = recBuffer.getHeader(&header);

    if (ret != SUCCESS)
        return ret;

    header.numEntries++;

    ret = recBuffer.setHeader(&header);

    if (ret != SUCCESS)
        return ret;
    
    //increment noofrecords in relation
    relCatEntry.numRecs++;

    ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;

}

int BlockAccess::search(int relId, Attribute *record,char attrName[ATTR_SIZE],Attribute attrVal,int op){
    RecId recId;
    recId=BlockAccess::linearSearch(relId,attrName,attrVal,op);
    if(recId.block==-1 && recId.slot==-1){
        return E_NOTFOUND;
    }
    RecBuffer recBuffer(recId.block);
    int ret=recBuffer.getRecord(record,recId.slot);
    if(ret!=SUCCESS){
        return ret;
    }
    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal,relName);

    char relcatAttrRelName[ATTR_SIZE]=RELCAT_ATTR_RELNAME;
    RecId relCatRecId=linearSearch(RELCAT_RELID,relcatAttrRelName,relNameAttr,EQ);

    if(relCatRecId.block==-1 && relCatRecId.slot==-1){
        return E_RELNOTEXIST;
    }

    Attribute relcatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBuffer(relCatRecId.block);
    int ret=relCatBuffer.getRecord(relcatEntryRecord,relCatRecId.slot);
    if(ret!=SUCCESS) return ret;

    int firstBlock=(int)relcatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs=(int)relcatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    (void)numAttrs;

    //delete all the record blocks of relation
    int currentBlock=firstBlock;
    while(currentBlock!=-1){
        RecBuffer currentBuffer(currentBlock);
        HeadInfo header;
        ret=currentBuffer.getHeader(&header);
        if(ret!=SUCCESS) return ret;
        //get the next block
        currentBlock=header.rblock;
        currentBuffer.releaseBlock();
    }
    //deleting attribute catalog entries corresponding the relation
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    char attrcatAttrRelName[ATTR_SIZE]=ATTRCAT_ATTR_RELNAME;
    int numberofAttributesDeleted=0;

    while(true){
        RecId attrCatRecId;
        attrCatRecId=linearSearch(ATTRCAT_RELID,attrcatAttrRelName,relNameAttr,EQ);
        if(attrCatRecId.block==-1 && attrCatRecId.slot==-1){
            break;
        }
        numberofAttributesDeleted++;
        RecBuffer attrCatBuffer(attrCatRecId.block);
        HeadInfo header;
        ret=attrCatBuffer.getHeader(&header);
        if(ret!=SUCCESS) return ret;

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        ret=attrCatBuffer.getRecord(attrCatRecord,attrCatRecId.slot);
        if(ret!=SUCCESS) return ret;

        unsigned char slotmap[header.numSlots];
        ret=attrCatBuffer.getSlotMap(slotmap);
        if(ret!=SUCCESS) return ret;
        slotmap[attrCatRecId.slot]=SLOT_UNOCCUPIED;
        ret=attrCatBuffer.setSlotMap(slotmap);
        if(ret!=SUCCESS) return ret;
        header.numEntries--;
        ret=attrCatBuffer.setHeader(&header);
        if(ret!=SUCCESS) return ret;

        if(header.numEntries==0){
            RecBuffer leftBuffer(header.lblock);
            HeadInfo leftHeader;
            ret=leftBuffer.getHeader(&leftHeader);
            if(ret!=SUCCESS) return ret;
            leftHeader.rblock=header.rblock;
            ret=leftBuffer.setHeader(&leftHeader);
            if(ret!=SUCCESS) return ret;

            if(header.rblock!=-1){
                RecBuffer rightBuffer(header.rblock);
                HeadInfo rightHeader;
                ret=rightBuffer.getHeader(&rightHeader);
                if(ret!=SUCCESS) return ret;
                rightHeader.lblock=header.lblock;
                ret=rightBuffer.setHeader(&rightHeader);
                if(ret!=SUCCESS) return ret;
            } else {
                RelCatEntry attrCatRelEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID,&attrCatRelEntry);
                attrCatRelEntry.lastBlk=header.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID,&attrCatRelEntry);
            }
            attrCatBuffer.releaseBlock();
            RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
        }
    }
     //delete the entry corresponding to the relation from relation catalog
    HeadInfo relCatHeader;
    ret=relCatBuffer.getHeader(&relCatHeader);
    if(ret!=SUCCESS) return ret;

    relCatHeader.numEntries--;
    ret = relCatBuffer.setHeader(&relCatHeader);
    if (ret != SUCCESS) return ret;

    // mark the slot as free in the relation catalog slot map
    unsigned char relCatSlotMap[relCatHeader.numSlots];
    ret = relCatBuffer.getSlotMap(relCatSlotMap);
    if (ret != SUCCESS) return ret;
    relCatSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    ret = relCatBuffer.setSlotMap(relCatSlotMap);
    if (ret != SUCCESS) return ret;

    /*** Updating the Relation Cache Table ***/
    // relation catalog: number of records decreases by 1
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

    // attribute catalog: number of records decreases by numberOfAttributesDeleted
    RelCatEntry attrCatEntry;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
    attrCatEntry.numRecs -= numberofAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);

    return SUCCESS;
}