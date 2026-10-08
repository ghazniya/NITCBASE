#include "Schema.h"
#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]){
    int rel=OpenRelTable::openRel(relName);
    if(rel>=0){
        return SUCCESS;
    }
    return rel;
}
/*
 * Renaming is only allowed on a closed, non-catalog relation: the caches would
 * otherwise still hold the old name.
 */
int Schema::renameRel(char oldRelName[ATTR_SIZE],char newRelName[ATTR_SIZE]){
    if(strcmp(oldRelName,RELCAT_RELNAME)==0 || strcmp(oldRelName,ATTRCAT_RELNAME)==0 ||
       strcmp(newRelName,RELCAT_RELNAME)==0 || strcmp(newRelName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }

    if(OpenRelTable::getRelId(oldRelName)!=E_RELNOTOPEN){
        return E_RELOPEN;
    }

    return BlockAccess::renameRelation(oldRelName,newRelName);
}

int Schema::renameAttr(char relName[ATTR_SIZE],char oldAttrName[ATTR_SIZE],char newAttrName[ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }

    if(OpenRelTable::getRelId(relName)!=E_RELNOTOPEN){
        return E_RELOPEN;
    }

    return BlockAccess::renameAttribute(relName,oldAttrName,newAttrName);
}

int Schema::closeRel(char relName[ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }
    int relId=OpenRelTable::getRelId(relName);
    if(relId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    return OpenRelTable::closeRel(relId);
}

int Schema::createRel(char relName[],int nAttrs,char attrs[][ATTR_SIZE],int attrtype[]){
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal,relName);

    //checking whether the relation exist in the database
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    char relcatAttrRelName[ATTR_SIZE]=RELCAT_ATTR_RELNAME;
    RecId targetRelId=BlockAccess::linearSearch(RELCAT_RELID,relcatAttrRelName,relNameAsAttribute,EQ);
    if(targetRelId.block!=-1 && targetRelId.slot!=-1){
        return E_RELEXIST;
    }

    //duplicate attribute
    for(int i=0;i<nAttrs;i++){
        for(int j=i+1;j<nAttrs;j++){
            if(strcmp(attrs[i],attrs[j])==0){
                return E_DUPLICATEATTR;
            }
        }
    }

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal=nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal=0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal=-1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal=-1;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal=floor(2016/(16*nAttrs+1));

    int retVal=BlockAccess::insert(RELCAT_RELID,relCatRecord);
    if(retVal!=SUCCESS){
        return retVal;
    }

    for(int i=0;i<nAttrs;i++){
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,attrs[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal=attrtype[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal=-1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal=-1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal=i;

        retVal=BlockAccess::insert(ATTRCAT_RELID,attrCatRecord);
        if(retVal!=SUCCESS){
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }
    return SUCCESS;
}

int Schema::deleteRel(char *relName){
    if(strcmp(relName,RELCAT_RELNAME)==0 || strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }

    int relId=OpenRelTable::getRelId(relName);
    if(relId>=0 && relId<MAX_OPEN){
        return E_RELOPEN;
    }
    return BlockAccess::deleteRelation(relName);
}