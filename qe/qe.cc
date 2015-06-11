
#include "qe.h"

Filter::Filter(Iterator* input, const Condition &condition) {
    in = input;
    filterCondition = condition;
    //string attr = condition.bRhsIsAttr ? condition.rhsAttr : condition.lhsAttr;

    // initializes to return all attributes
    vector<Attribute> attrs;
    in->getAttributes(attrs);

    // Search for left and right condition positions
    for(unsigned i = 0; i < attrs.size(); ++i)
    {
        if (attrs[i].name == condition.lhsAttr) {
            leftConditionAttr = attrs[i];
            leftConditionPos = i;
        }

        if (condition.bRhsIsAttr && attrs[i].name == condition.rhsAttr) {
            rightConditoinAttr = attrs[i];
            rightConditionPos = i;
        }

    }
}

RC Filter::getNextTuple(void *data) {
    // loop until a tuple is found that satisfies the condition
    vector<Attribute> attrs;
    in->getAttributes(attrs);

    int offset = 1;

    void* leftValue = malloc(PAGE_SIZE);
    void* rightValue = malloc(PAGE_SIZE);

    while (in->getNextTuple(data) != -1) {
        // search for condition value
        for (int i = 0; i <  attrs.size(); i++) {
            // TODO Handle Nulls
            // we are at the position, test its value
            if (leftConditionPos == i) {
                switch (leftConditionAttr.type) {
                case TypeInt: memcpy((char*)leftValue, data + offset, sizeof(int));
                    break;
                case TypeReal: memcpy((char*)leftValue, data + offset, sizeof(float));
                    break;
                case TypeVarChar: memcpy((char*)leftValue, data + offset, leftConditionAttr.length + sizeof(int));
                    break;
                }

            }

            if (filterCondition.bRhsIsAttr && rightConditionPos == i) {
                switch (rightConditoinAttr.type) {
                case TypeInt: memcpy((char*)rightValue, data + offset, sizeof(int));
                    break;
                case TypeReal: memcpy((char*)rightValue, data + offset, sizeof(float));
                    break;
                case TypeVarChar: memcpy((char*)rightValue, data + offset, rightConditoinAttr.length + sizeof(int));
                    break;
                }
            }

            // skip over the attribute
            switch (attrs[i].type) {
                case TypeInt:
                    offset += sizeof(int);
                    break;
                case TypeReal:
                    offset += sizeof(float);
                    break;
                case TypeVarChar:
                    int length;
                    memcpy(&length, (char*)data + offset, sizeof(int));
                    offset += sizeof(int) + length;
                    break;
                }
        }

        if ((filterCondition.bRhsIsAttr && compareValues(leftValue, rightValue))
            || compareValues(leftValue, filterCondition.rhsValue.data)){
            return 0;
        }

        // reset offset
        offset = 1;
    }

    // if it reached this point, we have reached the end of the tuples
    return -1;
}

bool Filter::compareValues(void *left, void* right) {
    switch (leftConditionAttr.type) {
    case TypeInt:
        int leftInt;
        memcpy(&leftInt, (char *) left, sizeof(int));

        int rightInt;
        memcpy(&rightInt, (char *) right, sizeof(int));

        switch(filterCondition.op) {
            case EQ_OP:     return leftInt == rightInt;
            case LT_OP:     return leftInt < rightInt;
            case GT_OP:     return leftInt > rightInt;
            case LE_OP:     return leftInt <= rightInt;
            case GE_OP:     return leftInt >= rightInt;
            case NE_OP:     return leftInt != rightInt;
            case NO_OP:     return true;
            default:        return false;
        }
        break;
    case TypeReal:
        float leftReal;
        memcpy(&leftReal, (char *) left, sizeof(float));

        float rightReal;
        memcpy(&rightReal, (char *) right, sizeof(float));

        switch(filterCondition.op) {
            case EQ_OP:     return leftReal == rightReal;
            case LT_OP:     return leftReal < rightReal;
            case GT_OP:     return leftReal > rightReal;
            case LE_OP:     return leftReal <= rightReal;
            case GE_OP:     return leftReal >= rightReal;
            case NE_OP:     return leftReal != rightReal;
            case NO_OP:     return true;
            default:        return false;
        }

        break;
    case TypeVarChar:
        int leftLength;
        memcpy(&leftLength, (char *) left, sizeof(int));
        char* leftVal = new char[leftLength + 1];
        memcpy(leftVal, (char *) left + sizeof(int), leftLength);
        leftVal[leftLength] = '\0';
        string leftVarChar = std::string(leftVal);

        int rightLength;
        memcpy(&rightLength, (char *) right, sizeof(int));
        char* rightVal = new char[rightLength + 1];
        memcpy(rightVal, (char *) right + sizeof(int), rightLength);
        rightVal[rightLength] = '\0';
        string rightVarChar = std::string(rightVal);

        switch(filterCondition.op) {
            case EQ_OP:     return leftVarChar == rightVarChar;
            case LT_OP:     return leftVarChar < rightVarChar;
            case GT_OP:     return leftVarChar > rightVarChar;
            case LE_OP:     return leftVarChar <= rightVarChar;
            case GE_OP:     return leftVarChar >= rightVarChar;
            case NE_OP:     return leftVarChar != rightVarChar;
            case NO_OP:     return true;
            default:        return false;
        }

        break;
    }

    return 0;
}

Project::Project(Iterator* input, const vector<string> &attrNames) {
    in = input;
    attrs = attrNames;
    Value v;
    v.data = NULL;

    // Conver to vector of strings
    vector<string> returnAttrs;
    unsigned i;
    for(i = 0; i < attrs.size(); ++i)
    {
        attrs.at(i).erase(0, attrs.at(i).find(".") + 1);
    }

    if (dynamic_cast<TableScan*>(in)) {
        static_cast<TableScan*>(in)->setIterator(NO_OP, "", attrs, v);
    } else if (dynamic_cast<IndexScan*>(in)) {
       // static_cast<IndeScan*>(in)->set
    } else if (dynamic_cast<BNLJoin*>(in)) {

    } else if (dynamic_cast<INLJoin*>(in)) {

    }
}

// ... the rest of your implementations go here
RC Project::getNextTuple(void *data) {
    // here we can use in and cond to do stuff
    if (dynamic_cast<TableScan*>(in)) {
        if (static_cast<TableScan*>(in)->getNextTuple(data) == -1) {
            return -1;
        }
    } else if (dynamic_cast<IndexScan*>(in)) {
        if (static_cast<IndexScan*>(in)->getNextTuple(data) == -1) {
            return -1;
        }
    } else if (dynamic_cast<BNLJoin*>(in)) {
        if (static_cast<IndexScan*>(in)->getNextTuple(data) == -1) {
            return -1;
        }
    } else if (dynamic_cast<INLJoin*>(in)) {
        if (static_cast<INLJoin*>(in)->getNextTuple(data) == -1) {
            return -1;
        }
    }
}

/*
 * Aggregate section
 */
Aggregate::Aggregate(Iterator *input,          // Iterator of input R
                  Attribute aggAttr,        // The attribute over which we are computing an aggregate
                  AggregateOp op            // Aggregate operation
        ) {
    setIterator(input);
    setAttribute(aggAttr);
    setOperator(op);
    setValue(0);
};

// Aggregate getNextTuple will only collect reals and ints (NO VARCHARS)
RC Aggregate::getNextTuple(void *data) {
    void *buffer = malloc(PAGE_SIZE);
    int counter = 0; // used for AVG
    float aggregateValue = 0;
    int intTemp;
    float floatTemp;
    bool aggregateFound = false;

    // aggregate values
    vector<Attribute> attrs;
    getAttributes(attrs);
    int offset = 1;

    // loop over all tuples and aggregate the aggregate operator
    switch (getOperator()) {
        case MAX:
            while (getIterator()->getNextTuple(buffer) != RM_EOF) {
                aggregateFound = true;
                // iterate over the attributes and find the aggregate attribute
                for (Attribute attr: attrs) {
                    if (attr.name == getAttribute().name) {
                        switch (getAttribute().type) {
                            case TypeInt: memcpy(&intTemp, buffer + offset, sizeof(int));
                                if (intTemp > aggregateValue) {
                                    aggregateValue = (float)intTemp;
                                }
                                break;
                            case TypeReal: memcpy(&floatTemp, buffer + offset, sizeof(float));
                                if (floatTemp > aggregateValue) {
                                    aggregateValue = floatTemp;
                                }
                                break;
                            }
                    }

                    switch (getAttribute().type) {
                        case TypeInt: offset += sizeof(int);
                            break;
                        case TypeReal: offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int len;
                            memcpy(&len, (char*)buffer + offset, sizeof(int));
                            offset += sizeof(int) + len;
                            break;
                    }
                }

                // reset offset
                offset = 1;
            }
            break;
        case MIN:
            while (getIterator()->getNextTuple(buffer) != RM_EOF) {
                aggregateFound = true;
                // iterate over the attributes and find the aggregate attribute
                for (Attribute attr: attrs) {
                    if (attr.name == getAttribute().name) {
                        switch (getAttribute().type) {
                            case TypeInt: memcpy(&intTemp, buffer + offset, sizeof(int));
                                if (intTemp < aggregateValue) {
                                    aggregateValue = (float)intTemp;
                                }
                                break;
                            case TypeReal: memcpy(&floatTemp, buffer + offset, sizeof(float));
                                if (floatTemp < aggregateValue) {
                                    aggregateValue = floatTemp;
                                }
                                break;
                            }
                    }

                    switch (getAttribute().type) {
                        case TypeInt: offset += sizeof(int);
                            break;
                        case TypeReal: offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int len;
                            memcpy(&len, (char*)buffer + offset, sizeof(int));
                            offset += sizeof(int) + len;
                            break;
                    }
                }

                // reset offset
                offset = 1;
            }
            break;
        case SUM:
            while (getIterator()->getNextTuple(buffer) != RM_EOF) {
                aggregateFound = true;
                // iterate over the attributes and find the aggregate attribute
                for (Attribute attr: attrs) {
                    if (attr.name == getAttribute().name) {
                        switch (getAttribute().type) {
                            case TypeInt: memcpy(&intTemp, buffer + offset, sizeof(int));
                                aggregateValue += (float)intTemp;
                                break;
                            case TypeReal: memcpy(&floatTemp, buffer + offset, sizeof(float));
                                aggregateValue += floatTemp;
                                break;
                            }
                    }

                    switch (getAttribute().type) {
                        case TypeInt: offset += sizeof(int);
                            break;
                        case TypeReal: offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int len;
                            memcpy(&len, (char*)buffer + offset, sizeof(int));
                            offset += sizeof(int) + len;
                            break;
                    }
                }

                // reset offset
                offset = 1;
            }
            break;
        case AVG:
            while (getIterator()->getNextTuple(buffer) != RM_EOF) {
                aggregateFound = true;
                // iterate over the attributes and find the aggregate attribute
                for (Attribute attr: attrs) {
                    if (attr.name == getAttribute().name) {
                        switch (getAttribute().type) {
                        case TypeInt: memcpy(&intTemp, buffer + offset, sizeof(int));
                            aggregateValue += (float)intTemp;
                            break;
                        case TypeReal: memcpy(&floatTemp, buffer + offset, sizeof(float));
                            aggregateValue += floatTemp;
                            break;
                        }
                    }

                    switch (getAttribute().type) {
                        case TypeInt: offset += sizeof(int);
                            break;
                        case TypeReal: offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int len;
                            memcpy(&len, (char*)buffer + offset, sizeof(int));
                            offset += sizeof(int) + len;
                            break;
                    }
                }

                // reset offset
                offset = 1;
                counter++;
            }
            break;
        case COUNT:
            while (getIterator()->getNextTuple(buffer) != RM_EOF) {
                aggregateValue++;
            }
            break;
        default:
            return -1;
    }

    if (!aggregateFound) {
        return -1;
    }

    // set aggregate value for data
    if (getOperator() == AVG) aggregateValue = aggregateValue / (float) counter;
    memcpy((char*)data + 1, &aggregateValue, sizeof(float));
    return 0;
}

void Aggregate::getAttributes(vector<Attribute> &attrs) const {
    return aggregateIterator->getAttributes(attrs);
}

/*
 * Block Nested Loop Join section
 */
BNLJoin::BNLJoin(Iterator *leftIn,            // Iterator of input R
        TableScan *rightIn,           // TableScan Iterator of input S
        const Condition &condition,   // Join condition
        const unsigned numRecords     // # of records can be loaded into memory, i.e., memory block size (decided by the optimizer)
         ) {
    setLeftIterator(leftIn);
    setRightIterator(rightIn);
    setNumRecords(numRecords);

    string attr = condition.bRhsIsAttr ? condition.rhsAttr : condition.lhsAttr;

    // initializes all the left return attributes
    vector<Attribute> leftAttrs;
    getLeftIterator()->getAttributes(leftAttrs);

    // initializes all the right return attributes
    vector<Attribute> rightAttrs;
    getRightIterator()->getAttributes(rightAttrs);

    // get the left join attribute
    for (int i = 0; i < leftAttrs.size(); i++) {
        if (leftAttrs[i].name == condition.lhsAttr) {
            setLeftJoinAttribute(leftAttrs[i]);
        }
    }

    // get the right join attribute
    for (int i = 0; i < rightAttrs.size(); i++) {
        if (rightAttrs[i].name == condition.rhsAttr) {
            setRightJoinAttribute(rightAttrs[i]);
        }
    }

    // set attrs lengths
    setLeftNumAttrs(leftAttrs.size());
    setRightNumAttrs(rightAttrs.size());
}

RC BNLJoin::getNextTuple(void *data) {
    // TODO We may need to delete buffers in map on clear
    int counter = 0;

    // Update the map
    if (innerFinished) {
        innerFinished = false;
        bool reachedEnd = false;
        int numRecords = getNumRecords();
        void *buffer = malloc(PAGE_SIZE);

        switch (getLeftJoinAttribute().type) {
            case TypeInt:
                while (getLeftIterator()->getNextTuple(buffer) != -1) {
                    int returnInt;
                    int bufferSize;
                    int offset = 0;

                    // iterate over buffer, collect the correct attribute, then finish iterating
                    // and store the buffer size
                    vector<Attribute> attrs;
                    getLeftIterator()->getAttributes(attrs);

                    // adjust for nullindicator size using ceiling function
                    offset = 1 + ((attrs.size() - 1) / 8);

                    // malloc the new buffer
                    void *buf = malloc(PAGE_SIZE);

                    for (int i = 0; i < attrs.size(); i++) {
                        if (!RecordBasedFileManager::isFieldNull(buffer, i)) {
                            if (attrs[i].name == getLeftJoinAttribute().name) {
                                memcpy(&returnInt, (char*)buffer + offset, sizeof(int));
                            }

                            // skip over the attribute
                            switch (attrs[i].type) {
                            case TypeInt:
                                offset += sizeof(int);
                                break;
                            case TypeReal:
                                offset += sizeof(float);
                                break;
                            case TypeVarChar:
                                int length;
                                memcpy(&length, (char*)buffer + offset, sizeof(int));
                                offset += sizeof(int) + length;
                                break;
                            }
                        }
                    }

                    // save bufferSize
                    bufferSize = offset;
                    void *entryBuffer = malloc(bufferSize);
                    memcpy(entryBuffer, buffer, bufferSize);
                    // store into map
                    //int hash = intHashFunction(returnInt, numRecords);

                    auto hashVal = intHashMap.find(returnInt);
                    intMapEntry entry = { returnInt, entryBuffer, bufferSize };

                    // check and see if the tableID already exists in the map
                    if (hashVal == intHashMap.end()) {
                        vector<intMapEntry> v { entry };
                        intHashMap[returnInt] = v;
                    } else {
                        hashVal->second.push_back(entry);
                    }

                    counter++;

                    if (counter >= numRecords) {
                        break;
                    }
                }
                break;
            case TypeReal:
                while (getLeftIterator()->getNextTuple(buffer) != -1) {
                    float returnReal;
                    int bufferSize;
                    int offset = 0;

                    // iterate over buffer, collect the correct attribute, then finish iterating
                    // and store the buffer size
                    vector<Attribute> attrs;
                    getLeftIterator()->getAttributes(attrs);

                    // adjust for nullindicator size using ceiling function
                    offset = 1 + ((attrs.size() - 1) / 8);

                    // malloc the new buffer
                    void *buf = malloc(PAGE_SIZE);

                    for (int i = 0; i < attrs.size(); i++) {
                        if (!RecordBasedFileManager::isFieldNull(buffer, i)) {
                            if (attrs[i].name == getLeftJoinAttribute().name) {
                                memcpy(&returnReal, (char*)buffer + offset, sizeof(int));
                            }

                            // skip over the attribute
                            switch (attrs[i].type) {
                            case TypeInt:
                                offset += sizeof(int);
                                break;
                            case TypeReal:
                                offset += sizeof(float);
                                break;
                            case TypeVarChar:
                                int length;
                                memcpy(&length, (char*)buffer + offset, sizeof(int));
                                offset += sizeof(int) + length;
                                break;
                            }
                        }
                    }

                    // save bufferSize
                    bufferSize = offset;
                    void *entryBuffer = malloc(bufferSize);
                    memcpy(entryBuffer, buffer, bufferSize);

                    // store into map
                    //int hash = intHashFunction(returnInt, numRecords);

                    auto hashVal = realHashMap.find(returnReal);
                    realMapEntry entry = { returnReal, entryBuffer, bufferSize };

                    // check and see if the tableID already exists in the map
                    if (hashVal == realHashMap.end()) {
                        vector<realMapEntry> v { entry };
                        realHashMap[returnReal] = v;
                    } else {
                        hashVal->second.push_back(entry);
                    }

                    counter++;

                    if (counter >= numRecords) {
                        break;
                    }
                }
                break;
            case TypeVarChar:
                while (getLeftIterator()->getNextTuple(buffer) != -1) {
                    string returnVarChar;
                    int bufferSize;
                    int offset = 0;

                    // iterate over buffer, collect the correct attribute, then finish iterating
                    // and store the buffer size
                    vector<Attribute> attrs;
                    getLeftIterator()->getAttributes(attrs);

                    // adjust for nullindicator size using ceiling function
                    offset = 1 + ((attrs.size() - 1) / 8);

                    // malloc the new buffer
                    void *buf = malloc(PAGE_SIZE);

                    for (int i = 0; i < attrs.size(); i++) {
                        if (!RecordBasedFileManager::isFieldNull(buffer, i)) {
                            if (attrs[i].name == getLeftJoinAttribute().name) {
                                int length;
                                int loffset = 1;
                                memcpy(&length, (char*)buffer + loffset, sizeof(int));
                                loffset += sizeof(int);
                                char* value = new char[length + 1];
                                memcpy(value, (char*)buffer + loffset, length);
                                loffset += length;
                                value[length] = '\0';
                                returnVarChar = std::string(value);

                                memcpy(&returnVarChar, (char*)buffer + offset, sizeof(int));
                            }

                            // skip over the attribute
                            switch (attrs[i].type) {
                            case TypeInt:
                                offset += sizeof(int);
                                break;
                            case TypeReal:
                                offset += sizeof(float);
                                break;
                            case TypeVarChar:
                                int length;
                                memcpy(&length, (char*)buffer + offset, sizeof(int));
                                offset += sizeof(int) + length;
                                break;
                            }
                        }
                    }

                    // save bufferSize
                    bufferSize = offset;
                    void *entryBuffer = malloc(bufferSize);
                    memcpy(entryBuffer, buffer, bufferSize);

                    // store into map
                    //int hash = intHashFunction(returnInt, numRecords);

                    auto hashVal = varCharHashMap.find(returnVarChar);
                    varCharMapEntry entry = { returnVarChar, entryBuffer, bufferSize };

                    // check and see if the tableID already exists in the map
                    if (hashVal == varCharHashMap.end()) {
                        vector<varCharMapEntry> v { entry };
                        varCharHashMap[returnVarChar] = v;
                    } else {
                        hashVal->second.push_back(entry);
                    }

                    counter++;

                    if (counter >= numRecords) {
                        break;
                    }
                }
                break;
            default:
                return -1;
        }

        free(buffer);

        // No more records left in left table
        if (counter == 0) {
            return -1;
        }
    }

    // Iterate over the right table and return the first tuple that exists in the memory
    void *rightBuffer = malloc(PAGE_SIZE);
    while (getRightIterator()->getNextTuple(rightBuffer) != -1) {
        switch (getRightJoinAttribute().type) {
            case TypeInt: {
                int returnInt;
                int bufferSize;
                int offset = 0;

                // iterate over buffer, collect the correct attribute, then finish iterating
                // and store the buffer size
                vector<Attribute> attrs;
                getRightIterator()->getAttributes(attrs);

                // adjust for nullindicator size using ceiling function
                offset = 1 + ((attrs.size() - 1) / 8);

                for (int i = 0; i < attrs.size(); i++) {
                    if (!RecordBasedFileManager::isFieldNull(rightBuffer, i)) {
                        if (attrs[i].name == getRightJoinAttribute().name) {
                            memcpy(&returnInt, (char*)rightBuffer + offset, sizeof(int));
                        }

                        // skip over the attribute
                        switch (attrs[i].type) {
                        case TypeInt:
                            offset += sizeof(int);
                            break;
                        case TypeReal:
                            offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int length;
                            memcpy(&length, (char*)rightBuffer + offset, sizeof(int));
                            offset += sizeof(int) + length;
                            break;
                        }
                    }
                }

                // save bufferSize
                bufferSize = offset;

                // test to see if the attribute exists in the map
                auto hashVal = intHashMap.find(returnInt);

                // hash has been found
                if (!(hashVal == intHashMap.end())) {
                    // iterate over list and attempt to find key
                    for (intMapEntry entry : hashVal->second) {
                        if (entry.attr == returnInt) {
                            joinBufferData(entry.buffer, entry.size, rightBuffer, bufferSize, data);
                            free(rightBuffer);
                            return 0;
                        }
                    }
                }
                break;
            }
            case TypeReal: {
                float returnReal;
                int bufferSize;
                int offset = 0;

                // iterate over buffer, collect the correct attribute, then finish iterating
                // and store the buffer size
                vector<Attribute> attrs;
                getRightIterator()->getAttributes(attrs);

                // adjust for nullindicator size using ceiling function
                offset = 1 + ((attrs.size() - 1) / 8);

                for (int i = 0; i < attrs.size(); i++) {
                    if (!RecordBasedFileManager::isFieldNull(rightBuffer, i)) {
                        if (attrs[i].name == getRightJoinAttribute().name) {
                            memcpy(&returnReal, (char*)rightBuffer + offset, sizeof(float));
                        }

                        // skip over the attribute
                        switch (attrs[i].type) {
                        case TypeInt:
                            offset += sizeof(int);
                            break;
                        case TypeReal:
                            offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int length;
                            memcpy(&length, (char*)rightBuffer + offset, sizeof(int));
                            offset += sizeof(int) + length;
                            break;
                        }
                    }
                }

                // save bufferSize
                bufferSize = offset;

                // test to see if the attribute exists in the map
                auto hashVal = realHashMap.find(returnReal);

                // hash has been found
                if (!(hashVal == realHashMap.end())) {
                    // iterate over list and attempt to find key
                    for (realMapEntry entry : hashVal->second) {
                        if (entry.attr == returnReal) {
                            joinBufferData(entry.buffer, entry.size, rightBuffer, bufferSize, data);
                            free(rightBuffer);
                            return 0;
                        }
                    }
                }
                break;
            }
            case TypeVarChar: {
                string returnVarChar;
                int bufferSize;
                int offset = 0;

                // iterate over buffer, collect the correct attribute, then finish iterating
                // and store the buffer size
                vector<Attribute> attrs;
                getRightIterator()->getAttributes(attrs);

                // adjust for nullindicator size using ceiling function
                offset = 1 + ((attrs.size() - 1) / 8);

                for (int i = 0; i < attrs.size(); i++) {
                    if (!RecordBasedFileManager::isFieldNull(rightBuffer, i)) {
                        if (attrs[i].name == getRightJoinAttribute().name) {
                            int length;
                            int loffset = 1;
                            memcpy(&length, (char*)rightBuffer + loffset, sizeof(int));
                            loffset += sizeof(int);
                            char* value = new char[length + 1];
                            memcpy(value, (char*)rightBuffer + loffset, length);
                            loffset += length;
                            value[length] = '\0';
                            returnVarChar = std::string(value);

                            memcpy(&returnVarChar, (char*)rightBuffer + offset, sizeof(int));
                        }

                        // skip over the attribute
                        switch (attrs[i].type) {
                        case TypeInt:
                            offset += sizeof(int);
                            break;
                        case TypeReal:
                            offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            int length;
                            memcpy(&length, (char*)rightBuffer + offset, sizeof(int));
                            offset += sizeof(int) + length;
                            break;
                        }
                    }
                }

                // save bufferSize
                bufferSize = offset;

                // test to see if the attribute exists in the map
                auto hashVal = varCharHashMap.find(returnVarChar);

                // hash has been found
                if (!(hashVal == varCharHashMap.end())) {
                    // iterate over list and attempt to find key
                    for (varCharMapEntry entry : hashVal->second) {
                        if (entry.attr == returnVarChar) {
                            joinBufferData(entry.buffer, entry.size, rightBuffer, bufferSize, data);
                            free(rightBuffer);
                            return 0;
                        }
                    }
                }
                break;
            }
        }
    }

    free(rightBuffer);

    // if we reached this point, then we need to refresh the memory and start again
    innerFinished = true;
    intHashMap.clear();
    realHashMap.clear();
    varCharHashMap.clear();

    // restart the right iterator
    TableScan *tc = new TableScan(getRightIterator()->rm, getRightIterator()->tableName);

    // TODO Does this need to be deleted?
    getRightIterator()->~TableScan();
    setRightIterator(tc);

    return getNextTuple(data);
}

void BNLJoin::getAttributes(vector<Attribute> &attrs) const {
    vector<Attribute> leftAttrs;
    getLeftIterator()->getAttributes(leftAttrs);
    vector<Attribute> rightAttrs;
    getRightIterator()->getAttributes(rightAttrs);

    attrs.reserve( leftAttrs.size() + rightAttrs.size());
    attrs.insert( attrs.end(), leftAttrs.begin(), leftAttrs.end());
    attrs.insert( attrs.end(), rightAttrs.begin(), rightAttrs.end());
}

RC BNLJoin::joinBufferData(void *buffer1, int buffer1Len, void* buffer2, int buffer2Len, void* data) {
    // create a new null indicator for merged entries
    int leftAttrsCount = getLeftNumAttrs();
    int rightAttrsCount = getRightNumAttrs();
    int leftIndicatorSize = 1 + (( leftAttrsCount - 1) / 8);
    int rightIndicatorSize = 1 + (( rightAttrsCount - 1) / 8);
    int indicatorSize = 1 + (((leftAttrsCount + rightAttrsCount) - 1) / 8);

    unsigned char *nullsIndicator = (unsigned char *) malloc(indicatorSize);
    memset(nullsIndicator, 0, indicatorSize);

    // initialize the indicator
    // for left indicator, just copy all the bytes straight over.
    memcpy((char*)nullsIndicator, buffer1, leftIndicatorSize);

    // iterate over all bits for rightAttrs, then merge them into nullsIndicator
    int position = leftAttrsCount;
    for (int i = 0; i < rightAttrsCount; i++) {
        // read the fragmented byte
        char currentByte;
        memcpy(&currentByte, nullsIndicator + (leftIndicatorSize - 1), sizeof(char));

        // add in the remaining bits using the right indicator
        char rightByte;
        for (int j = position % 8; j < 8 && i < rightAttrsCount; i++, j++, position++) {
            //char mask = 128 >> j;
            //currentByte |= mask;

            // get bit of right buffer at position i
            memcpy(&rightByte, buffer2 + (i / 8), sizeof(char));
            char mask = 128 >> i % 8;

            // clear other bits that we don't want
            char temp = rightByte & mask > 0 ? 128 : 0;

            // merge rightByte and currentByte
            temp >>= j;
            currentByte |= temp;
        }

        // copy back into nulls Indicator
        memcpy((char*)nullsIndicator + (position / 8), &currentByte, sizeof(char));
    }

    // return combine null indicator with 2 buffers (minus their seperate indicators)
    int offset = 0;
    memcpy((char*)data + offset, nullsIndicator, indicatorSize);
    offset += indicatorSize;
    memcpy((char*)data + offset, (char*)buffer1 + leftIndicatorSize, buffer1Len - leftIndicatorSize);
    offset += buffer1Len - leftIndicatorSize;
    memcpy((char*)data + offset, (char*)buffer2 + rightIndicatorSize, buffer2Len - rightIndicatorSize);

    return 0;
}

int BNLJoin::intHashFunction(int data, int numRecords) {
    return data % numRecords;
}

int BNLJoin::realHashFunction(float data, int numRecords) {
    unsigned int ui;
    memcpy(&ui, &data, sizeof(float));
    return ui & 0xfffff000;
}

int BNLJoin::varCharHashFunction(string data, int numRecords) {
    int length = data.size();
    char c[length + 1];
    strcpy(c, data.c_str());


    int sum = 0;
    for (int i = 0; i < length; i++) {
        sum += c[i];
    }

    return sum % numRecords;
}






