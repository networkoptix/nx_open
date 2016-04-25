//
//  HDWMap.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWCameraModel.h"

typedef int64_t KeyType;
typedef HDWCameraServerItem* ValueType;

@interface HDWMap : NSObject

- (instancetype)init;

- (void)insertObject:(id)object forKey:(KeyType)key;
- (NSEnumerator *)enumerateValues;

- (id)serverForExactDate:(KeyType)elem;
- (id)onlinServerForFutureDate:(KeyType)elem withChunks:(NSDictionary *)serverChunks;

@end
