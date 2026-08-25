#include "OpenRelTable.h"
#include<cstdlib>
#include <cstring>

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

    //student relation
    const int STUDENTS_RELID = 2;

    Attribute studentsRelCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(studentsRelCatRecord, 2);

    RelCacheEntry studentsRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(studentsRelCatRecord,&studentsRelCacheEntry.relCatEntry);
    studentsRelCacheEntry.recId.block = RELCAT_BLOCK;
    studentsRelCacheEntry.recId.slot  = 2;

    RelCacheTable::relCache[STUDENTS_RELID] =(RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[STUDENTS_RELID]) = studentsRelCacheEntry;

    // ---------- Attribute Cache entries for Students ----------
    // RELATIONCAT  -> slots 0-5
    // ATTRIBUTECAT -> slots 6-11
    // Students     -> slots 12-15

    head = nullptr;
    prev = nullptr;

    for (int i = 12; i < 16; i++) {
        Attribute studentsAttrRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(studentsAttrRecord, i);

        struct AttrCatEntry attrCatEntry;
        AttrCacheTable::recordToAttrCatEntry(studentsAttrRecord, &attrCatEntry);

        struct AttrCacheEntry *attrCacheEntry =(struct AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        attrCacheEntry->attrCatEntry  = attrCatEntry;
        attrCacheEntry->recId.block   = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot    = i;
        attrCacheEntry->next          = nullptr;

        if (i == 12) {
            head = attrCacheEntry;
        } else {
            prev->next = attrCacheEntry;
        }
        prev = attrCacheEntry;
    }

    AttrCacheTable::attrCache[STUDENTS_RELID] = head;
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
    free(RelCacheTable::relCache[RELCAT_RELID]);
    free(RelCacheTable::relCache[ATTRCAT_RELID]);

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
  if (strcmp(relName, RELCAT_RELNAME) == 0) {
    return RELCAT_RELID;
  }

  if (strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return ATTRCAT_RELID;
  }

  if (strcmp(relName, "Students") == 0) {
    return 2;
  }

  return E_RELNOTOPEN;
}