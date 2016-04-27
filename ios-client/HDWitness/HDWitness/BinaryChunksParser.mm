//
//  BinaryChunksParser.c
//  HDWitness
//
//  Created by Ivan Vigasin on 6/26/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#include "BinaryChunksParser.h"

#include <stdio.h>

#include <Foundation/Foundation.h>

qint64 decodeValue(const quint8 *&curPtr, const quint8 *end)
{
    if (curPtr >= end)
        return -1;
    int fieldSize = 2 + (*curPtr >> 6);
    if (end-curPtr < fieldSize)
        return -1;
    qint64 value = *curPtr++ & 0x3f;
    for (int i = 0; i < fieldSize-1; ++i)
        value = (value << 8) + *curPtr++;
    if (value == 0x3fffffffffll) {
        if (end - curPtr < 6)
            return -1;
        value = *curPtr++;
        for (int i = 0; i < 5; ++i)
            value = (value << 8) + *curPtr++;
    }
    return value;
};

bool decodeBinaryChunks(const quint8 *data, int dataSize, NSMutableArray* result)
{
    [result removeAllObjects];
    
    const quint8* curPtr = data;
    const quint8* end = data + dataSize;
    if (end - curPtr < 6)
        return false;
    
    qint64 fullStartTime = 0;
    memcpy(&fullStartTime, curPtr, 6);
    curPtr += 6;
    
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    fullStartTime <<= 16;
#else
    fullStartTime >>= 16;
#endif
    
    fullStartTime = NSSwapBigLongLongToHost(fullStartTime);
    
    qint64 relStartTime = 0;
    while (relStartTime != -1)
    {
        qint64 duration = decodeValue(curPtr, end);
        if (duration == -1)
            return false;
        duration--;
        fullStartTime += relStartTime;
        [result addObject:@[@(fullStartTime), @(duration)]];
        fullStartTime += duration;
        relStartTime = decodeValue(curPtr, end);
    }
    return true;
}

