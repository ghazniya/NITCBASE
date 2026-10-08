#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];
OpenRelTable::OpenRelTable(){
    for(int i=0;i<MAX_OPEN;i++){
        RelCacheTable::relCache[i]=nullptr;
        AttrCacheTable::attrCache[i]=nullptr;
        tableMetaInfo[i].free=true;
    }
    //relation catalog in relation cache table
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord,RELCAT_SLOTNUM_FOR_RELCAT);

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord,&relCacheEntry.relCatEntry);
        relCacheEntry.recId.block=RELCAT_BLOCK;
        relCacheEntry.recId.slot=RELCAT_SLOTNUM_FOR_RELCAT;
        relCacheEntry.dirty=false;

        RelCacheTable::relCache[RELCAT_RELID]=(struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[RELCAT_RELID])=relCacheEntry;

    // attrbute catalog in relation cache table
    RecBuffer attrRelCatBlock(RELCAT_BLOCK);
    Attribute attrRelCatRecord[ATTRCAT_NO_ATTRS];
    attrRelCatBlock.getRecord(attrRelCatRecord,RELCAT_SLOTNUM_FOR_ATTRCAT);
    
    struct RelCacheEntry attrRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(attrRelCatRecord,&attrRelCacheEntry.relCatEntry);
        attrRelCacheEntry.recId.block=ATTRCAT_BLOCK;
        attrRelCacheEntry.recId.slot=RELCAT_SLOTNUM_FOR_ATTRCAT;
        attrRelCacheEntry.dirty=false;
    RelCacheTable::relCache[ATTRCAT_RELID]=(struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID])=attrRelCacheEntry;



    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry* head=nullptr;
    AttrCacheEntry* prev=nullptr;

    for(int i=0;i<RELCAT_NO_ATTRS;i++){
        attrCatBlock.getRecord(attrCatRecord,i);
        struct AttrCatEntry attrCatEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&attrCatEntry);
        struct AttrCacheEntry* attrCacheEntry=(struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCacheEntry->attrCatEntry=attrCatEntry;
        attrCacheEntry->recId.block=ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot=i;
        attrCacheEntry->next=nullptr;
        if(i==0){
            head=attrCacheEntry;
        }else{
            prev->next=attrCacheEntry;
        }
        prev=attrCacheEntry;
    }
    AttrCacheTable::attrCache[RELCAT_RELID]=head;

       // Create linked list for ATTRCAT attributes (slots 6-11)
    head = nullptr;
    prev = nullptr;
    
    for(int i = 0; i < ATTRCAT_NO_ATTRS; i++){
        attrCatBlock.getRecord(attrCatRecord, RELCAT_NO_ATTRS + i);  // Offset by RELCAT attrs
        
        struct AttrCatEntry attrCatEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCatEntry);
        
        struct AttrCacheEntry* attrCacheEntry = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCacheEntry->attrCatEntry = attrCatEntry;
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = RELCAT_NO_ATTRS + i;
        attrCacheEntry->next = nullptr;
        
        if(i == 0){
            head = attrCacheEntry;
        } else {
            prev->next = attrCacheEntry;
        }
        prev = attrCacheEntry;
    }
    
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;  

    tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);

}

OpenRelTable::~OpenRelTable() {
    
    for(int i = 2; i < MAX_OPEN; i++){
        if(!tableMetaInfo[i].free){
            OpenRelTable::closeRel(i);
        }
    }

    if(RelCacheTable::relCache[ATTRCAT_RELID]->dirty){
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry),record);
        RecId recId=RelCacheTable::relCache[ATTRCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(record,recId.slot);
    }
    free(RelCacheTable::relCache[ATTRCAT_RELID]);
    RelCacheTable::relCache[ATTRCAT_RELID]=nullptr;

    if(RelCacheTable::relCache[RELCAT_RELID]->dirty){
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[RELCAT_RELID]->relCatEntry),record);

        RecId recId=RelCacheTable::relCache[RELCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(record,recId.slot);
    }
    free(RelCacheTable::relCache[RELCAT_RELID]);
    RelCacheTable::relCache[RELCAT_RELID]=nullptr;

    for(int relId = 0; relId <= 1; relId++){
        AttrCacheEntry* current = AttrCacheTable::attrCache[relId];
        while(current != nullptr){
            AttrCacheEntry* next = current->next;
            free(current);
            current = next;
        }
        AttrCacheTable::attrCache[relId] = nullptr;
    }
}
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    //printf("DEBUG getRelId: looking for '%s'\n", relName);
    for(int relid=0;relid<MAX_OPEN;relid++){
        if(strcmp(tableMetaInfo[relid].relName,relName)==0 && tableMetaInfo[relid].free==false){
            return relid;
        }
    }
  return E_RELNOTOPEN;
}
int OpenRelTable::getFreeOpenRelTableEntry(){
    for(int i=0;i<MAX_OPEN;i++){
        if(tableMetaInfo[i].free){
            return i;
        }
    }
    return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]){
    int existrelid=OpenRelTable::getRelId(relName);
    if(existrelid>=0){
        return existrelid;
    }
    int relid=OpenRelTable::getFreeOpenRelTableEntry();
    if(relid<0){
        return E_CACHEFULL;
    }
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal,relName);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId relcatRecId=BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAttr,EQ);

    if(relcatRecId.block==-1 && relcatRecId.slot==-1){
        return E_RELNOTEXIST;
    }
    RecBuffer relCatBlock(relcatRecId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord,relcatRecId.slot);

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord,&relCacheEntry.relCatEntry);
    relCacheEntry.recId=relcatRecId;
    relCacheEntry.dirty=false;

    RelCacheTable::relCache[relid]=(struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[relid])=relCacheEntry;

    AttrCacheEntry *listHead=nullptr;
    AttrCacheEntry *prev=nullptr;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    while(true){
        RecId attrcatRecId=BlockAccess::linearSearch(ATTRCAT_RELID,(char *)ATTRCAT_ATTR_RELNAME,relNameAttr,EQ);
        if(attrcatRecId.block==-1 && attrcatRecId.slot==-1){
            break;
        }
        RecBuffer attrCatBlock(attrcatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord,attrcatRecId.slot);
        struct AttrCacheEntry *entry=(struct AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&entry->attrCatEntry);
        entry->recId=attrcatRecId;
        entry->dirty=false;
        entry->next=nullptr;
        if(listHead==nullptr)listHead=entry;
        else prev->next=entry;
        prev=entry;
    }
    AttrCacheTable::attrCache[relid]=listHead;
    tableMetaInfo[relid].free=false;
    strcpy(tableMetaInfo[relid].relName,relName);
     //printf("DEBUG openRel: relid=%d stored='%s' free=%d\n",
           //relid, tableMetaInfo[relid].relName, tableMetaInfo[relid].free);
    return relid;
}
int OpenRelTable::closeRel(int relId){
    if(relId==RELCAT_RELID || relId==ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }

    if(relId<0 || relId>=MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(tableMetaInfo[relId].free){
        return E_RELNOTOPEN;
    }

    if(RelCacheTable::relCache[relId]->dirty==true){
        union Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&RelCacheTable::relCache[relId]->relCatEntry,record);
        RecId recId=RelCacheTable::relCache[relId]->recId;
        RecBuffer RelCatEntry(recId.block);
        RelCatEntry.setRecord(record,recId.slot);
        
    }
    free(RelCacheTable::relCache[relId]);
    AttrCacheEntry *current=AttrCacheTable::attrCache[relId];
    while(current!=nullptr){
        AttrCacheEntry *next=current->next;
        free(current);
        current=next;
    }
    AttrCacheTable::attrCache[relId]=nullptr;
    tableMetaInfo[relId].free=true;

    return SUCCESS;
}