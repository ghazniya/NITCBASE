#include "BPlusTree.h"

#include <cstring>

extern int totalnoofcomparison;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    IndexId searchIndex;

    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        // Start at the root for a new search.
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {
            return RecId{-1, -1};
        }

    } else {
        // Resume immediately after the previously matched leaf entry.
        block = searchIndex.block;
        index = searchIndex.index + 1;

        IndLeaf leaf(block);

        HeadInfo leafHead;

        leaf.getHeader(&leafHead);

        if (index >= leafHead.numEntries) {
            // Continue with the next leaf when the current leaf is exhausted.
            block = leafHead.rblock;
            index = 0;
            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        IndInternal internalBlk(block);

        HeadInfo intHead;
        internalBlk.getHeader(&intHead);
        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {
            // These operators require scanning from the leftmost leaf.
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;

        } else {
            int i = 0;
            while (i < intHead.numEntries) {
                internalBlk.getEntry(&intEntry, i);
                totalnoofcomparison++;
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                if (
                    (op == EQ && cmpVal >= 0) ||
                    (op == GE && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)
                ) {
                    break;
                }
                i++;
            }

            if (i != intHead.numEntries) {
                block = intEntry.lChild;

            } else {
                i = intHead.numEntries - 1;
                internalBlk.getEntry(&intEntry, i);
                block = intEntry.rChild;
            }
        }
    }

    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        leafBlk.getHeader(&leafHead);
        Index leafEntry;
        while (index < leafHead.numEntries) {

            leafBlk.getEntry(&leafEntry, index);
            totalnoofcomparison++;
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ) {
                searchIndex.block = block;
                searchIndex.index = index;
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

                return RecId{leafEntry.block, leafEntry.slot};

            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                return RecId {-1, -1};
            }

            ++index;
        }

        if (op != NE) {
            break;
        }

        // Continue through linked leaves for NE searches.
        block = leafHead.rblock;
        index = 0;
    }

    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId,char attrName[ATTR_SIZE]){
    if(relId==RELCAT_RELID || relId==ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }
    AttrCatEntry attrCatEntry;
    int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);
    if(ret!=SUCCESS) return ret;
    
    if(attrCatEntry.rootBlock!=-1) return SUCCESS;

    //allotes a new leaf block
    IndLeaf rootBlockBuf;
    int rootBlock=rootBlockBuf.getBlockNum();
    if(rootBlock==E_DISKFULL) return E_DISKFULL;

    //updates the attrcat with the rootblock no recieved
    attrCatEntry.rootBlock=rootBlock;
    ret=AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatEntry);
    if(ret!=SUCCESS) return ret;

    //get relation catalog entry
    RelCatEntry relCatEntry;
    ret=RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(ret!=SUCCESS) return ret;

    int block=relCatEntry.firstBlk;
    while(block!=-1){
        //traverse through blocks
        RecBuffer recBuffer(block);
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        recBuffer.getSlotMap(slotMap);
        for(int slot=0;slot<relCatEntry.numSlotsPerBlk;slot++){
            //get the slots
            if(slotMap[slot]==SLOT_UNOCCUPIED)
                continue;
            Attribute record[relCatEntry.numAttrs];
            recBuffer.getRecord(record,slot);
            RecId recId={block,slot};
            //inserting the attribute to the b+ tree
            ret=bPlusInsert(relId,attrName,record[attrCatEntry.offset],recId);
            if(ret==E_DISKFULL) return E_DISKFULL;
        }
        //next block of the relation
        HeadInfo header;
        recBuffer.getHeader(&header);
        block=header.rblock;

    }
    return SUCCESS;

}

int BPlusTree::bPlusDestroy(int rootBlockNum){
    if(rootBlockNum<0 || rootBlockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    int type=StaticBuffer::getStaticBlockType(rootBlockNum);
    if(type==IND_LEAF){
        //relese the leaf block
        IndLeaf leafBlk(rootBlockNum);
        leafBlk.releaseBlock();
        return SUCCESS;
    } else if(type==IND_INTERNAL){
        //get the header and destory its left and right children
        IndInternal internalBlk(rootBlockNum);
        HeadInfo header;
        internalBlk.getHeader(&header);
        InternalEntry entry;
        internalBlk.getEntry(&entry,0);
        BPlusTree::bPlusDestroy(entry.lChild);
        for(int i=0;i<header.numEntries;i++){
            internalBlk.getEntry(&entry,i);
            BPlusTree::bPlusDestroy(entry.rChild);
        }
        //releseing internal node
        internalBlk.releaseBlock();
        return SUCCESS;
    }else{
        return E_INVALIDBLOCK;
    }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {

    // get the attribute catalog entry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    int blockNum = attrCatEntry.rootBlock;

    // no index on this attribute
    if (blockNum == -1) {
        return E_NOINDEX;
    }

    // walk down the tree to the leaf where attrVal belongs
    int leafBlkNum = findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);

    // build the leaf entry: value + location of its record
    Index entry;
    entry.attrVal = attrVal;
    entry.block = recId.block;
    entry.slot = recId.slot;

    // insert (handles leaf split / internal split / new root)
    ret = insertIntoLeaf(relId, attrName, leafBlkNum, entry);

    if (ret == E_DISKFULL) {
        // the root may have changed during the insert (createNewRoot)
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

        // destroy the half-built index
        bPlusDestroy(attrCatEntry.rootBlock);

        // mark the attribute as not indexed
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {

        IndInternal internalBlk(blockNum);

        HeadInfo header;
        internalBlk.getHeader(&header);

        InternalEntry entry;
        int i = 0;

        // find the first entry whose key >= attrVal
        while (i < header.numEntries) {
            internalBlk.getEntry(&entry, i);
            if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0) {
                break;
            }
            i++;
        }

        if (i == header.numEntries) {
            // every key < attrVal → go to the rightmost child
            internalBlk.getEntry(&entry, header.numEntries - 1);
            blockNum = entry.rChild;
        } else {
            // found key >= attrVal → go to its left child
            blockNum = entry.lChild;
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    IndLeaf leafBlk(blockNum);

    HeadInfo blockHeader;
    leafBlk.getHeader(&blockHeader);

    // existing entries + the new one
    Index indices[blockHeader.numEntries + 1];

    /*** build the sorted array ***/
    bool inserted = false;
    int j = 0;                                   // position in indices[]

    for (int i = 0; i < blockHeader.numEntries; i++) {
        Index entry;
        leafBlk.getEntry(&entry, i);

        // place the new entry before the first existing entry that is larger
        if (!inserted &&
            compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntry.attrType) > 0) {
            indices[j++] = indexEntry;
            inserted = true;
        }
        indices[j++] = entry;
    }

    // new entry is the largest → goes at the end
    if (!inserted) {
        indices[j++] = indexEntry;
    }

    /*** case 1: it fits ***/
    if (blockHeader.numEntries != MAX_KEYS_LEAF) {
        blockHeader.numEntries++;
        leafBlk.setHeader(&blockHeader);

        for (int i = 0; i < blockHeader.numEntries; i++) {
            leafBlk.setEntry(&indices[i], i);
        }

        return SUCCESS;
    }

    /*** case 2: overflow → split ***/
    int newRightBlk = splitLeaf(blockNum, indices);

    if (newRightBlk == E_DISKFULL) {
        return E_DISKFULL;
    }

    int ret;

    if (blockHeader.pblock != -1) {
        // leaf had a parent → add the separating key to it
        InternalEntry newEntry;
        newEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;   // last value of left half
        newEntry.lChild = blockNum;
        newEntry.rChild = newRightBlk;

        ret = insertIntoInternal(relId, attrName, blockHeader.pblock, newEntry);
    } else {
        // leaf was the root → make a new root above the two leaves
        ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal,
                            blockNum, newRightBlk);
    }

    if (ret == E_DISKFULL) {
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {

    // constructor 1 → allocates a NEW leaf block
    IndLeaf rightBlk;

    // constructor 2 → opens the EXISTING leaf block
    IndLeaf leftBlk(leafBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    /*** right block header ***/
    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;   // 32
    rightBlkHeader.pblock = leftBlkHeader.pblock;           // same parent (for now)
    rightBlkHeader.lblock = leftBlkNum;                     // left neighbour = old leaf
    rightBlkHeader.rblock = leftBlkHeader.rblock;           // takes over old leaf's right link
    rightBlk.setHeader(&rightBlkHeader);

    /*** left block header ***/
    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;    // 32
    leftBlkHeader.rblock = rightBlkNum;                     // now points to the new leaf
    leftBlk.setHeader(&leftBlkHeader);

    /*** distribute the entries ***/
    for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++) {
        leftBlk.setEntry(&indices[i], i);                                // indices[0..31]
        rightBlk.setEntry(&indices[i + (MAX_KEYS_LEAF + 1) / 2], i);     // indices[32..63]
    }

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    IndInternal intBlk(intBlockNum);

    HeadInfo blockHeader;
    intBlk.getHeader(&blockHeader);

    InternalEntry internalEntries[blockHeader.numEntries + 1];

    /*** build the sorted array ***/
    int insertedIndex = -1;
    int j = 0;

    for (int i = 0; i < blockHeader.numEntries; i++) {
        InternalEntry entry;
        intBlk.getEntry(&entry, i);

        // put the new entry before the first existing key that is larger
        if (insertedIndex == -1 &&
            compareAttrs(entry.attrVal, intEntry.attrVal, attrCatEntry.attrType) > 0) {
            internalEntries[j] = intEntry;
            insertedIndex = j;
            j++;
        }
        internalEntries[j++] = entry;
    }

    if (insertedIndex == -1) {          // new key is the largest → goes last
        internalEntries[j] = intEntry;
        insertedIndex = j;
        j++;
    }

    // the entry right after the new one must now have the new block as its left child
    if (insertedIndex < blockHeader.numEntries) {
        internalEntries[insertedIndex + 1].lChild = intEntry.rChild;
    }

    /*** case 1: it fits ***/
    if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
        blockHeader.numEntries++;
        intBlk.setHeader(&blockHeader);

        for (int i = 0; i < blockHeader.numEntries; i++) {
            intBlk.setEntry(&internalEntries[i], i);
        }

        return SUCCESS;
    }

    /*** case 2: overflow → split ***/
    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL) {
        // the subtree under the new right child isn't linked into the tree yet → free it
        bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    int ret;

    if (blockHeader.pblock != -1) {
        // push the middle key up into the parent
        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum;
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        middleEntry.rChild = newRightBlk;

        ret = insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    } else {
        // this was the root → make a new root
        ret = createNewRoot(relId, attrName,
                            internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
                            intBlockNum, newRightBlk);
    }

    if (ret == E_DISKFULL) {
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {

    //allocates a NEW internal block
    IndInternal rightBlk;

    //opens the EXISTING internal block
    IndInternal leftBlk(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    //int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    /*** right block header ***/
    rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;    // 50
    rightBlkHeader.pblock = leftBlkHeader.pblock;         // same parent
    rightBlk.setHeader(&rightBlkHeader);

    /*** left block header ***/
    leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;     // 50
    leftBlk.setHeader(&leftBlkHeader);

    /*** distribute entries: 0..49 left, 50 goes up, 51..100 right ***/
    for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++) {
        leftBlk.setEntry(&internalEntries[i], i);
        rightBlk.setEntry(&internalEntries[i + MIDDLE_INDEX_INTERNAL + 1], i);
    }

    /*** children that moved to the right block must point to it as parent ***/
    

    // first child of the right block
    BlockBuffer firstChild(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);
    HeadInfo childHeader;
    firstChild.getHeader(&childHeader);
    childHeader.pblock = rightBlkNum;
    firstChild.setHeader(&childHeader);

    // rChild of every entry in the right block
    for (int i = MIDDLE_INDEX_INTERNAL + 1; i <= MAX_KEYS_INTERNAL; i++) {
        BlockBuffer child(internalEntries[i].rChild);
        child.getHeader(&childHeader);
        childHeader.pblock = rightBlkNum;
        child.setHeader(&childHeader);
    }

    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    // constructor 1 → allocate a NEW internal block
    IndInternal newRootBlk;

    int newRootBlkNum = newRootBlk.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        // rChild was just created by the split and isn't connected to anything → free it
        bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    /*** header: one key ***/
    HeadInfo rootHeader;
    newRootBlk.getHeader(&rootHeader);
    rootHeader.numEntries = 1;
    newRootBlk.setHeader(&rootHeader);

    /*** the single entry: lChild | attrVal | rChild ***/
    InternalEntry entry;
    entry.lChild = lChild;
    entry.attrVal = attrVal;
    entry.rChild = rChild;
    newRootBlk.setEntry(&entry, 0);

    /*** both children now have the new root as parent ***/
    BlockBuffer leftChildBlk(lChild);
    HeadInfo leftHeader;
    leftChildBlk.getHeader(&leftHeader);
    leftHeader.pblock = newRootBlkNum;
    leftChildBlk.setHeader(&leftHeader);

    BlockBuffer rightChildBlk(rChild);
    HeadInfo rightHeader;
    rightChildBlk.getHeader(&rightHeader);
    rightHeader.pblock = newRootBlkNum;
    rightChildBlk.setHeader(&rightHeader);

    /*** tell the attribute catalog where the tree starts now ***/
    attrCatEntry.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return SUCCESS;
}