
#include "rbfm.h"

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
    pfm = PagedFileManager::instance();
}

RecordBasedFileManager::~RecordBasedFileManager()
{

}

RC RecordBasedFileManager::createFile(const string &fileName) {
    return pfm->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    return pfm->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    return pfm->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return pfm->closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {  
    // lets determine if we need to append a new page or just write to a page
    int length = getRecordSize(data, recordDescriptor);

    // findOpenSlot() will search for an open slot in the slot directory 
    // if it finds one it will update the rid and return the new offset for the record
    int newOffset = findOpenSlot(fileHandle, length, rid); 
    if (newOffset == -1) {
        // first thing we need to do is write the current page to file and free up the memory
        // only if there is 1 or more pages in the file Handle
        if (fileHandle.getNumberOfPages() != 0 && fileHandle.currentPage != NULL) {
            if (fileHandle.writePage(fileHandle.getNumberOfPages() - 1, fileHandle.currentPage)) {
                // error writing to file
                return -1;
            }
            free(fileHandle.currentPage);
        }
        
        // we need to append a new page
        fileHandle.currentPage = malloc(PAGE_SIZE);
        void *newPage = fileHandle.currentPage;
        memset(newPage, 0, PAGE_SIZE);
        
        // update the RID
        rid.pageNum = fileHandle.currentPageNum = fileHandle.getNumberOfPages();
        rid.slotNum = 0;
        
        // Now let's add the new record
        int length = getRecordSize(data, recordDescriptor);
        setUpNewPage(newPage, data, length, fileHandle);
        fileHandle.appendPage(newPage);
        return 0;
    } else {
        
        // Determin if we will use the current page or a previous page
        void *page = NULL;
        if (fileHandle.currentPageNum == rid.pageNum) {
            page = fileHandle.currentPage;
        } else {
            page = malloc(PAGE_SIZE);
            fileHandle.readPage(rid.pageNum, page);
        }
        // move all the data over
        memcpy((char *) page + newOffset, (char *) data, length);
        
        // update the number of records and free space
        int numRecords;
        memcpy(&numRecords, (char *) page + N_OFFSET, sizeof(int));
        numRecords++;
        memcpy((char *) page + N_OFFSET, &numRecords, sizeof(int));

        int freeSpaceOffset;
        memcpy(&freeSpaceOffset, (char *) page + F_OFFSET, sizeof(int));
        freeSpaceOffset += length;
        int freeSpace = PAGE_SIZE - (freeSpaceOffset + (numRecords * SLOT_SIZE) + META_INFO);
        fileHandle.freeSpace[rid.pageNum] = freeSpace;
        memcpy((char *) page + F_OFFSET, &freeSpaceOffset, sizeof(int));
        
        // now we need to enter in the slot directory entry
        int slotEntryOffset = N_OFFSET - (numRecords * SLOT_SIZE); 
        memcpy((char *) page + slotEntryOffset, &newOffset, sizeof(int));
        memcpy((char *) page + slotEntryOffset + sizeof(int), &length, sizeof(int));

        fileHandle.writePage(rid.pageNum, page);
        // if we opened a page that was not the header page then free that memory
        if (fileHandle.currentPageNum != rid.pageNum) {
            free(page);
        }
        return 0; 
    }
    return -1;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    // First generate a page to transfer over from the file, using the pageNum
    if (rid.pageNum != fileHandle.currentPageNum) {
        fileHandle.currentPage = malloc(PAGE_SIZE);
        fileHandle.currentPageNum = rid.pageNum;
        fileHandle.readPage(rid.pageNum, fileHandle.currentPage);
    }

    void *page = fileHandle.currentPage;
    // we now need to get the page offset and data offset

    int offset;
    int length;
    getSlotFile(rid.slotNum, page, &offset, &length);

    // Now copy the entire contents into data
    memcpy((char *) data, (char *) page + offset, length);
    return 0;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    // Go through all the attributes and print the data
    std::string s;
    int numNullBytes = ceil((double)recordDescriptor.size() / CHAR_BIT);
    int offset = numNullBytes;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); ++it) {
        // test to see if the field is NULL
        int i = it - recordDescriptor.begin();
        s += it->name + ": ";
        s += isFieldNull(data, i) ? "NULL" : extractType(data, &offset, it->type, it->length);
        s += '\t';
    }
    cout << s << endl;
    return 0;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid) {
    // First generate a page to transfer over from the file, using the pageNum
    if (rid.pageNum != fileHandle.currentPageNum) {
        fileHandle.currentPage = malloc(PAGE_SIZE);
        fileHandle.currentPageNum = rid.pageNum;
        fileHandle.readPage(rid.pageNum, fileHandle.currentPage);
    }

    void *page = fileHandle.currentPage;

    int offset;
    int length;
    getSlotFile(rid.slotNum, page, &offset, &length);

    // Write uninitialized data into the page where the record currently lies
    void *newData = malloc(length);
    memcpy(page, (char*)newData, length);

    // Clear out the slot in the meta data
    int location = PAGE_SIZE - (((rid.slotNum + 1) * SLOT_SIZE) + META_INFO);
    memcpy(page, (char*)page + location, SLOT_SIZE);
    
    // Shifts the data appropriately
    compactMemory(offset, length, page);

    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid) {
    // First generate a page to transfer over from the file, using the pageNum
    if (rid.pageNum != fileHandle.currentPageNum) {
        fileHandle.currentPage = malloc(PAGE_SIZE);
        fileHandle.currentPageNum = rid.pageNum;
        fileHandle.readPage(rid.pageNum, fileHandle.currentPage);
    }

    RID tempRid;
    void *page = fileHandle.currentPage;

    // Delete the old record
    RecordBasedFileManager::deleteRecord(fileHandle, recordDescriptor, rid);

    // Get size of record
    int length = getRecordSize(data, recordDescriptor);

    // Get new offset and (potentially) new RID. RID could be new if the updated record is now too large for page.
    int offSet = findOpenSlot(fileHandle, length, tempRid);

    
    // If the new RID slot is on a different page, update the slot record with the negated version of these values
    if (rid.pageNum != tempRid.pageNum) {
        // because the first open slot is on a new page, just insert record as usual
        if (RecordBasedFileManager::insertRecord(fileHandle, recordDescriptor, data, tempRid) == -1) return -1;

        //update slot directory with negative values to reflect tombstone
        tempRid.pageNum *= -1;
        tempRid.slotNum *= -1;

        int slotEntryOffset = N_OFFSET - (rid.slotNum * SLOT_SIZE);
        memcpy((char *)page + slotEntryOffset, &tempRid.pageNum, sizeof(int));
        memcpy((char *)page + slotEntryOffset + sizeof(int), &tempRid.slotNum, sizeof(int));

        return 0;
    }
    else {
        // Write record
        memcpy((char*)page + offSet, data, length);

        // update slot directory
        int slotEntryOffset = N_OFFSET - (rid.slotNum * SLOT_SIZE);
        memcpy((char *)page + slotEntryOffset, &offSet, sizeof(int));
        memcpy((char *)page + slotEntryOffset + sizeof(int), &length, sizeof(int));

        return 0;
    }

    return -1;
}


bool RecordBasedFileManager::isFieldNull(const void *data, int i) {
    // create an bitmask to test if the field is null
    unsigned char *bitmask = (unsigned char*) malloc(1);
    memset(bitmask, 0, 1);
    *bitmask = 1 << 7;
    *bitmask >>= i % CHAR_BIT;
    
    // extract the NULL fields indicator from the data
    unsigned char *nullField = (unsigned char*) malloc(1);
    memcpy(nullField, (char *) data + (i / CHAR_BIT), 1);
    bool retVal = (*bitmask & *nullField) ? true : false;
    free(bitmask);
    free(nullField);
    return retVal;
}

std::string RecordBasedFileManager::extractType(const void *data, int *offset, AttrType t, AttrLength l) {
    if (t == TypeInt) {
        int value; 
        memcpy(&value, (char *) data + *offset, sizeof(int));
        *offset += sizeof(int);
        return std::to_string((long long) value);
    } else if (t == TypeReal) {
        float val;
        memcpy(&val, (char *) data + *offset, sizeof(float));
        *offset += sizeof(float);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << val;
        return ss.str();
    } else if (t == TypeVarChar) {
        // first extract the length of the char
        int varCharLength;
        memcpy(&varCharLength, (char *) data + *offset, sizeof(int));
        
        // now generate a C string with the same length plus 1
        *offset += sizeof(int);
        char* s = new char[varCharLength];
        memcpy(s, (char *) data + *offset, varCharLength);

        std::string str(s);
        *offset += varCharLength;

        delete s;
        return str;
    } else {
        // this shouldn't happen since we assume all incoming data is correct
        return "ERROR EXTRACTING"; 
    }
}


int getRecordSize(const void *data, const vector<Attribute> &descriptor) {
    int dataOffset = 0;
    // Copy null field 
    int numNullBytes = ceil((double)descriptor.size() / CHAR_BIT);
    dataOffset += numNullBytes;
    
    for (auto it = descriptor.begin(); it != descriptor.end(); ++it) {
        if (it->type == TypeInt) {
            dataOffset += sizeof(int);
        } else if (it->type == TypeReal) {
            dataOffset += sizeof(float);
        } else if (it->type == TypeVarChar) {
            int varCharLength;
            memcpy(&varCharLength, (char *) data + dataOffset, sizeof(int));
            dataOffset += sizeof(int) + varCharLength;
        } else {
            // this should not happen since we assume all data coming it is always correct, for now
        }
    } // end of for loop
    return dataOffset; 
}

void RecordBasedFileManager::getSlotFile(int slotNum, const void *page, int *offset, int *length) {
    // first lets get the slot offset 
    int location = PAGE_SIZE - (((slotNum + 1) * SLOT_SIZE) + META_INFO);
    memcpy(offset, (char *) page + location, sizeof(int)); 
    memcpy(length, (char *) page + location + sizeof(int), sizeof(int));
}


int RecordBasedFileManager::findOpenSlot(FileHandle &handle, int size, RID &rid) {
    // first we need to check and see if the current page has available space
    int pageNum = handle.getNumberOfPages() - 1;
    if (pageNum < 0) {
        // this means we have no pages in a file and must generate a page
        return -1;
    }
    // if we get here we have a page current page and we need to get its freespace
    void *page = handle.currentPage; 
    
    int freeSpace = handle.freeSpace[pageNum];
    if (freeSpace > (size + SLOT_SIZE)) {
        // the current page has enough space to fit a new record
        rid.pageNum = pageNum;
        return getFreeSpaceOffset(page, rid);
    }
    
    int sizeOfFile = handle.currentPageNum;
    int retVal = -1;
    
    // we only need to test the pages upto the current one, since we already tested it.
    for (int i = 0; i < sizeOfFile; i++) {
        freeSpace = handle.freeSpace[i];
        // if the free space is big enough to accomodate the new record then stick it in.
        if (freeSpace > (size + SLOT_SIZE)) {
            // open a temp page and scan it for a new offset
            void *_tempPage = malloc(PAGE_SIZE); 
            handle.readPage(i, _tempPage);
            rid.pageNum = i;
            retVal = getFreeSpaceOffset(_tempPage, rid);  
            free(_tempPage);
            break;
        }
    } 
    // if we get here than no space was available and we need to append
    return retVal;
}


int RecordBasedFileManager::getFreeSpaceOffset(const void *data, RID &rid) {
    int numRecords;
    memcpy(&numRecords, (char *) data + N_OFFSET, sizeof(int));
    rid.slotNum = numRecords; 
    
    int freeSpaceOffset;
    memcpy(&freeSpaceOffset, (char *) data + F_OFFSET, sizeof(int));    

    return freeSpaceOffset;
    
    /* 
    int lengthOfRecord = 0;
    int lastRecordOffset = 0;
    
    // lets move our pointer to the last directory slot and extract the
    // record offset and length and them up and return it to the caller
    int slotOffset = PAGE_SIZE - ((numRecords * SLOT_SIZE) + META_INFO);
    memcpy(&lastRecordOffset, (char *) data + slotOffset, sizeof(int));
    memcpy(&lengthOfRecord, (char *) data + slotOffset + sizeof(int), sizeof(int)); 
    return lastRecordOffset + lengthOfRecord; 
    */
}

void RecordBasedFileManager::setUpNewPage(const void *newPage, const void *data, int length, FileHandle &handle) {
    // for the next part we only want to know the length of the record and then copy all it's contents over
    memcpy((char *) newPage, (char *) data, length);
        
    // we need put 1 page in the slot directory meta data
    int numRecords = 1;
    memcpy((char *) newPage + N_OFFSET, &numRecords, sizeof(int));

    // next we need to add slot 1 meta data, each slot is 2 ints (8 bytes) in length to fit the offset and length
    int slotOneOffset = N_OFFSET - (2 * sizeof(int));;
        
    // enter the offset first which is zero because its the first record in a page
    int offset = 0;
    memcpy((char *) newPage + slotOneOffset, &offset, sizeof(int));
    slotOneOffset += sizeof(int);
    memcpy((char *) newPage + slotOneOffset, &length, sizeof(int));

    // have the FreeSpaceOffset point to the end of the first record
    int freeSpaceOffset = length;
    int freeSpace = PAGE_SIZE - (freeSpaceOffset + SLOT_SIZE + META_INFO);

    // lets setup the freeSpace list in th fileHandle, we don't need a page number 
    // because we are making a new page and we just append the end of the list
    handle.freeSpace.push_back(freeSpace); 
    memcpy((char *) newPage + F_OFFSET, &freeSpaceOffset, sizeof(int));
}


// TODO Fix Failed Test Case
RC RecordBasedFileManager::scan(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, 
                                        const string &conditionAttribute, const CompOp compOp, 
                                        const void *value, const vector<string> &attributeNames,
                                        RBFM_ScanIterator &rbfm_ScanIterator) {
    // first lets attach the fileHandle to the scanner iterater
    rbfm_ScanIterator.handle = &fileHandle;
    rbfm_ScanIterator.compOp = compOp;
    rbfm_ScanIterator.currentOffset = 0;
    rbfm_ScanIterator.descriptor = &recordDescriptor;
    rbfm_ScanIterator.descSize = recordDescriptor.size();
    rbfm_ScanIterator.value = value;

    // add the the first page to scanPage and set pageNum and slotNUm
    fileHandle.readPage(rbfm_ScanIterator.pageNum, rbfm_ScanIterator.scanPage); 

    // collect the attribute placements for each record
    int i;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); ++it) {
        i = it - recordDescriptor.begin();
        for (auto itN = attributeNames.begin(); itN != attributeNames.end(); ++itN) {
            if (strcmp(it->name.c_str(), itN->c_str()) == 0) {
                rbfm_ScanIterator.attrPlacement.push_back(i);
                if (strcmp(it->name.c_str(), conditionAttribute.c_str()) == 0) {
                        rbfm_ScanIterator.conditionAttribute = i;
                }
            }
        }
    } 
    return 0;
}


// get the next record
RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data) {
    // we have to check for empty slots
    

    // check for end of the page and load new page if needed
    

    // check for NULL Fields and make sure they are null
    
    // enter in the rid info
    rid.pageNum = pageNum;
    rid.slotNum = slotNum;

    // test the condition we need to extract
    bool isCompTrue;
    int condOffset = getAnyTypeOffset(*descriptor, scanPage, conditionAttribute, condType) + currentOffset;
    
    // here we needt to run the comparison functions with the data
    if (condType == TypeInt) {
        isCompTrue = processIntComp(condOffset, compOp, value, scanPage);
    } else if (condType == TypeReal) {
        isCompTrue = processFloatComp(condOffset, compOp, value, scanPage); 
    } else if (condType == TypeVarChar) {
        isCompTrue = processStringComp(condOffset, compOp, value, scanPage);
    } else {
        // this is bad
        return -1;
    }
    if (isCompTrue) {
        // extract the attributes
        extractScannedData(attrPlacement, *descriptor, scanPage, currentOffset, data); 
 
    } else {
        data = NULL;
    }
    currentOffset += getRecordSize(scanPage, *descriptor); 
    slotNum++;
    return 0;
}



int getAnyTypeOffset(const vector<Attribute> &descriptor, void *data, int cond, AttrType &type) {
    int place = 0;
    int dataOffset = 0;
    // Copy null field 
    int numNullBytes = ceil((double)descriptor.size() / CHAR_BIT);
    dataOffset += numNullBytes;
    
    for (auto it = descriptor.begin(); it == descriptor.end(); ++it) {
        if (it->type == TypeInt) {
            if (place == cond) {
                type = TypeInt;
                break;
            }
            dataOffset += sizeof(int);
            place++;
        } else if (it->type == TypeReal) {
            if (place == cond) {
                type = TypeReal;
                break;
            }
            dataOffset += sizeof(float);
            place++;
        } else if (it->type == TypeVarChar) {
            if (place == cond) {
                type = TypeVarChar;
                break;
            }
            int varCharLength;
            memcpy(&varCharLength, (char *) data + dataOffset, sizeof(int));
            dataOffset += sizeof(int) + varCharLength;
            place++;
        } else {
            // this should not happen since we assume all data coming it is always correct, for now
        }
    } // end of for loop
    return dataOffset;
}


bool processIntComp(int condOffset, CompOp compOp, const void *value, const void *page) {
    int intVal;
    memcpy(&intVal, (char *) value, sizeof(int));
    
    int recordVal;
    memcpy(&recordVal, (char *) page + condOffset, sizeof(int));
    
    bool returnVal;
    switch(compOp) {
        case 0:     returnVal = true;  
                    break;
        case 1:     returnVal = recordVal == intVal;    
                    break;
        case 2:     returnVal = recordVal < intVal; 
                    break;
        case 3:     returnVal = recordVal > intVal; 
                    break;
        case 4:     returnVal = recordVal <= intVal; 
                    break;
        case 5:     returnVal = recordVal >= intVal; 
                    break;
        case 6:     returnVal = recordVal != intVal; 
                    break;
        default:    returnVal = false;
                    break;
    }
    return returnVal;
}


bool processFloatComp(int condOffset, CompOp compOp, const void *value, const void *page) {
    float floatVal;
    memcpy(&floatVal, (char *) value, sizeof(float));
    
    float recordVal;
    memcpy(&recordVal, (char *) page + condOffset, sizeof(float));
    
    bool returnVal;
    switch(compOp) {
        case 0:     returnVal = true;  
                    break;
        case 1:     returnVal = recordVal == floatVal;    
                    break;
        case 2:     returnVal = recordVal < floatVal; 
                    break;
        case 3:     returnVal = recordVal > floatVal; 
                    break;
        case 4:     returnVal = recordVal <= floatVal; 
                    break;
        case 5:     returnVal = recordVal >= floatVal; 
                    break;
        case 6:     returnVal = recordVal != floatVal; 
                    break;
        default:    returnVal = false;
                    break;
    }
    return returnVal;
}


bool processStringComp(int condOffset, CompOp compOp, const void *value, const void *page) {
    int valueLength;
    memcpy(&valueLength, (char *) value , sizeof(int));
    
    // now generate a C string with the same length plus 1
    char* s = new char[valueLength + 1];
    memcpy(s, (char *) value + sizeof(int), valueLength);
    s[valueLength] = '\0';
    
    int varCharLength;
    memcpy(&varCharLength, (char *) page + condOffset, sizeof(int));

    char* sv = new char[varCharLength + 1];
    memcpy(&sv, (char *) page + sizeof(int) + condOffset, varCharLength);
    sv[varCharLength] = '\0';

    bool returnVal;
    switch(compOp) {
        case 0:     returnVal = true;  
                    break;
        case 1:     returnVal = strcmp(s, sv) == 0 ? true : false;    
                    break;
        case 2:     returnVal = strcmp(s, sv) < 0 ? true : false; 
                    break;
        case 3:     returnVal = strcmp(s, sv) > 0 ? true : false;
                    break;
        case 4:     returnVal = strcmp(s, sv) < 0 || strcmp(s, sv) == 0 ? true : false;
                    break;
        case 5:     returnVal = strcmp(s, sv) > 0 || strcmp(s, sv) == 0 ? true : false; 
                    break;
        case 6:     returnVal = strcmp(s, sv) != 0 ? true : false;
                    break;
        default:    returnVal = false;
                    break;
    }
    return returnVal;
}


void extractScannedData(vector<int> &placement
        , const vector<Attribute> &descriptor
        , void *page
        , int offset
        , void *data) {
    // go through each placement and extract that data
    int place = 0;
    int dataOffset = 0;
    int counter = 0;
    int cond = placement[counter];


    for (auto it = descriptor.begin(); it == descriptor.end(); ++it) {
        if (it->type == TypeInt) {
            if (place == cond) {
                memcpy((char *) data + dataOffset, (char *) page + offset, sizeof(int)); 
                cond = placement[++counter]; 
            }
            place++;
        } else if (it->type == TypeReal) {
            if (place == cond) {
                memcpy((char *) data + dataOffset, (char *) page + offset, sizeof(float));
                cond = placement[++counter];
            }
            offset += sizeof(float);
            dataOffset += sizeof(float);
            place++;
        } else if (it->type == TypeVarChar) {
            int varCharLength;
            if (place == cond) {
                memcpy(&varCharLength, (char *) page + offset, sizeof(int));
                memcpy((char *) data +dataOffset, &varCharLength, sizeof(int));
                memcpy((char *) data + dataOffset + sizeof(int), (char *) page + offset, varCharLength);
                cond = placement[++counter];
            } else {
                memcpy(&varCharLength, (char *) data + dataOffset, sizeof(int));
            }
            offset += sizeof(int) + varCharLength;
            dataOffset += sizeof(int) + varCharLength;
            place++;
        } else {
            // this should not happen since we assume all data coming it is always correct, for now
        }
    } // end of for loop

}

void RecordBasedFileManager::compactMemory(int offset, int length, const void *data) {

}