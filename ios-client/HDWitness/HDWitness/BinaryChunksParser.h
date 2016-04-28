//
//  BinaryChunksParser.h
//  HDWitness
//
//  Created by Ivan Vigasin on 6/26/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#ifndef HDWitness_BinaryChunksParser_h
#define HDWitness_BinaryChunksParser_h

typedef int64_t qint64;
typedef uint8_t quint8;

#if defined __cplusplus
extern "C" {
#endif
    
    bool decodeBinaryChunks(const quint8 *data, int dataSize, NSMutableArray* result);
    
#if defined __cplusplus
};
#endif

#endif
