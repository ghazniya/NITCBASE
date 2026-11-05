#include "Algebra.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern int totalnoofcomparison;

bool isNumber(char *str); 
int Algebra::select(char srcRel[ATTR_SIZE],char targetRel[ATTR_SIZE],char attr[ATTR_SIZE],int op,char strVal[ATTR_SIZE]){
    totalnoofcomparison = 0;
    int srcRelId=OpenRelTable::getRelId(srcRel);
    if(srcRelId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    AttrCatEntry attrCatEntry;
    int ret=AttrCacheTable::getAttrCatEntry(srcRelId,attr,&attrCatEntry);
    if(ret!=SUCCESS){
        return E_ATTRNOTEXIST;
    }
    int type=attrCatEntry.attrType;
    Attribute attrVal;
    if(type==NUMBER){
        if(isNumber(strVal)){
            attrVal.nVal=atof(strVal);
        }else{
            return E_ATTRTYPEMISMATCH;
        }
    }else if(type==STRING){
        strcpy(attrVal.sVal,strVal);
    }

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId,&relCatEntry);

    int src_nAttrs=relCatEntry.numAttrs;
    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];

    for(int i=0;i<src_nAttrs;i++){
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId,i,&attrCatEntry);
        strcpy(attr_names[i],attrCatEntry.attrName);
        attr_types[i]=attrCatEntry.attrType;
    }

    ret=Schema::createRel(targetRel,src_nAttrs,attr_names,attr_types);
    if(ret!=SUCCESS){
        return ret;
    }
    int targetRelId=OpenRelTable::openRel(targetRel);
    if(targetRelId<0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }
    //reset search index for the source relation and the attribute
    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId,attr);
    
    //search and insert the records matching
    Attribute record[src_nAttrs];
    while(BlockAccess::search(srcRelId,record,attr,attrVal,op)==SUCCESS){
        ret=BlockAccess::insert(targetRelId,record);
        if(ret!=SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }
    printf("total no of comparisons:%d\n",totalnoofcomparison);
    Schema::closeRel(targetRel);
    return SUCCESS;
}

bool isNumber(char *str){
    int len;
    float ignore;
    int ret=sscanf(str,"%f %n",&ignore,&len);
    return ret==1 && len == strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE],int nAttrs,char record[][ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }
    int relId=OpenRelTable::getRelId(relName);
    if(relId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    RelCatEntry relCatEntry;
    int retVal=RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(retVal!=SUCCESS){
        return retVal;
    }
    if(relCatEntry.numAttrs!=nAttrs){
        return E_NATTRMISMATCH;
    }

    union Attribute recordValues[nAttrs];
    for(int i=0;i<nAttrs;i++){
        AttrCatEntry attrCatEntry;
        retVal=AttrCacheTable::getAttrCatEntry(relId,i,&attrCatEntry);
        if(retVal!=SUCCESS){
            return retVal;
        }
        if(attrCatEntry.attrType==NUMBER){
            if(isNumber(record[i])){
                recordValues[i].nVal=atof(record[i]);
            }else{
                return E_ATTRTYPEMISMATCH;
            }
        }
        else if(attrCatEntry.attrType==STRING){
            strcpy(recordValues[i].sVal,record[i]);
        }
    }
    retVal=BlockAccess::insert(relId,recordValues);
    return retVal;
}

int Algebra::project(char srcRel[ATTR_SIZE],char targetRel[ATTR_SIZE]){
    int srcRelId=OpenRelTable::getRelId(srcRel);
    if(srcRelId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    RelCatEntry relCatEntry;
    int ret=RelCacheTable::getRelCatEntry(srcRelId,&relCatEntry);
    if(ret!=SUCCESS){
        return ret;
    }
    int src_nAttrs=relCatEntry.numAttrs;
    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];

    for(int i=0;i<src_nAttrs;i++){
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId,i,&attrCatEntry);
        strcpy(attr_names[i],attrCatEntry.attrName);
        attr_types[i]=attrCatEntry.attrType;
    }

    //creating target relation
    ret=Schema::createRel(targetRel,src_nAttrs,attr_names,attr_types);
    if(ret!=SUCCESS){
        return ret;
    }
    int targetRelId=OpenRelTable::openRel(targetRel);
    if(targetRelId<0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    //reset search index for the source relation
    RelCacheTable::resetSearchIndex(srcRelId);

    //search and insert the records
    Attribute record[src_nAttrs];
    while(BlockAccess::project(srcRelId,record)==SUCCESS){
        ret=BlockAccess::insert(targetRelId,record);
        if(ret!=SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }
    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE],char targetRel[ATTR_SIZE],int tar_nAttrs,char tar_Attrs[][ATTR_SIZE]){
    int srcRelId=OpenRelTable::getRelId(srcRel);
    if(srcRelId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    RelCatEntry relCatEntry;
    int ret=RelCacheTable::getRelCatEntry(srcRelId,&relCatEntry);
    if(ret!=SUCCESS){
        return ret;
    }
    int src_nAttrs=relCatEntry.numAttrs;

    //stores offset and types of attribute
    int attr_offsets[tar_nAttrs];
    int attr_types[tar_nAttrs];

    //cheking whether each attribute exist
    for(int i=0;i<tar_nAttrs;i++){
        AttrCatEntry attrCatEntry;
        ret=AttrCacheTable::getAttrCatEntry(srcRelId,tar_Attrs[i],&attrCatEntry);
        if(ret!=SUCCESS){
            return E_ATTRNOTEXIST;
        }
        attr_offsets[i]=attrCatEntry.offset;
        attr_types[i]=attrCatEntry.attrType;
    }

    ret=Schema::createRel(targetRel,tar_nAttrs,tar_Attrs,attr_types);
    if(ret!=SUCCESS){
        return ret;
    }
    int targetRelId=OpenRelTable::openRel(targetRel);
    if(targetRelId<0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[src_nAttrs];

    //fetch record from source relation
    while(BlockAccess::project(srcRelId,record)==SUCCESS){
        Attribute proj_record[tar_nAttrs];

        //fetching the needed attibutes
        for(int i=0;i<tar_nAttrs;i++){
            proj_record[i]=record[attr_offsets[i]];
        }

        //insert projected record
        ret=BlockAccess::insert(targetRelId,proj_record);
        if(ret!=SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }
    Schema::closeRel(targetRel);
    return SUCCESS;
}

//stage12:join

int Algebra::join(char srcRelation1[ATTR_SIZE],char srcRelation2[ATTR_SIZE],char targetRelation[ATTR_SIZE],char attribute1[ATTR_SIZE],char attribute2[ATTR_SIZE]){
    int relid1=OpenRelTable::getRelId(srcRelation1);
    int relid2=OpenRelTable::getRelId(srcRelation2);
    if(relid1==E_RELNOTOPEN ||relid2==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    AttrCatEntry attrCatEntry1,attrCatEntry2;
    int ret1=AttrCacheTable::getAttrCatEntry(relid1,attribute1,&attrCatEntry1);
    int ret2=AttrCacheTable::getAttrCatEntry(relid2,attribute2,&attrCatEntry2);
    if(ret1==E_ATTRNOTEXIST || ret2==E_ATTRNOTEXIST) return E_ATTRNOTEXIST;
    if(attrCatEntry1.attrType!=attrCatEntry2.attrType) return E_ATTRTYPEMISMATCH;
    RelCatEntry relCatEntry1,relCatEntry2;
    RelCacheTable::getRelCatEntry(relid1,&relCatEntry1);
    RelCacheTable::getRelCatEntry(relid2,&relCatEntry2);
    int numOfAttributes1=relCatEntry1.numAttrs;
    int numOfAttributes2=relCatEntry2.numAttrs;
    for(int i=0;i<numOfAttributes1;i++){
        AttrCatEntry attr1;
        AttrCacheTable::getAttrCatEntry(relid1,i,&attr1);
        for(int j=0;j<numOfAttributes2;j++){
            AttrCatEntry attr2;
            AttrCacheTable::getAttrCatEntry(relid2,j,&attr2);
            if(i==attrCatEntry1.offset && j==attrCatEntry2.offset){
                continue;
            }
            if(strcmp(attr1.attrName,attr2.attrName)==0){
                return E_DUPLICATEATTR;
            }
        }
    }
    
    if(attrCatEntry2.rootBlock==-1){
        int ret=BPlusTree::bPlusCreate(relid2,attribute2);
        if(ret!=SUCCESS) return ret;
    }
    
    int numOfAttributeInTarget=numOfAttributes1+numOfAttributes2-1;
    char targetRelAttrNames[numOfAttributeInTarget][ATTR_SIZE];
    int targetRelAttrTypes[numOfAttributeInTarget];
    int targetIndex=0;
    for(int i=0;i<numOfAttributes1;i++){
        AttrCatEntry attrEntry;
        AttrCacheTable::getAttrCatEntry(relid1,i,&attrEntry);
        strcpy(targetRelAttrNames[targetIndex],attrEntry.attrName);
        targetRelAttrTypes[targetIndex]=attrEntry.attrType;
        targetIndex++;
    }
    for(int i=0;i<numOfAttributes2;i++){
        if(i==attrCatEntry2.offset){
            continue;
        }
        AttrCatEntry attrEntry;
        AttrCacheTable::getAttrCatEntry(relid2,i,&attrEntry);
        strcpy(targetRelAttrNames[targetIndex],attrEntry.attrName);
        targetRelAttrTypes[targetIndex]=attrEntry.attrType;
        targetIndex++;
    }
    
    int ret=Schema::createRel(targetRelation,numOfAttributeInTarget,targetRelAttrNames,targetRelAttrTypes);
    if(ret!=SUCCESS){
        return ret;
    }

    int targetRelId=OpenRelTable::openRel(targetRelation);
    if(targetRelId<0){
        Schema::deleteRel(targetRelation);
        return targetRelId;
    }
    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];
    Attribute targetRecord[numOfAttributeInTarget];

    RelCacheTable::resetSearchIndex(relid1);
    while(BlockAccess::project(relid1,record1)==SUCCESS){

        RelCacheTable::resetSearchIndex(relid2);
        AttrCacheTable::resetSearchIndex(relid2,attribute2);
        while(BlockAccess::search(relid2,record2,attribute2,record1[attrCatEntry1.offset],EQ)==SUCCESS){
            int k=0;
            for(int i=0;i<numOfAttributes1;i++){
                targetRecord[k++]=record1[i];
            }
            for(int i=0;i<numOfAttributes2;i++){
                if(i==attrCatEntry2.offset) continue;
                targetRecord[k++]=record2[i];
            }
            ret=BlockAccess::insert(targetRelId,targetRecord);
            if(ret!=SUCCESS){
                OpenRelTable::closeRel(targetRelId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }
    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;

}