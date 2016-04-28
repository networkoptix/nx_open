//
//  HDWMap.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWMap.h"

#include <map>
#include <memory>

namespace  {
//    struct ObjectLess {
//        bool operator()(id obj1, id obj2) const {
//            return [obj1 compare:obj2] == NSOrderedAscending;
//        }
//    };

//    typedef std::map<id, id, ObjectLess> ObjectMap;
//    typedef std::unique_ptr<ObjectMap> ObjectMapPtr;
    typedef std::map<KeyType, ValueType> ObjectMap;
    typedef std::unique_ptr<ObjectMap> ObjectMapPtr;
}

//@interface HDWMapKeyEnumerator : NSEnumerator {
//    const ObjectMap *_map;
//    ObjectMap::const_iterator _iter;
//}
//
//- (id)initWithMap:(const ObjectMap *)map;
//@end
//
//@implementation HDWMapKeyEnumerator
//
//- (id)initWithMap:(const ObjectMap *)map {
//    self = [super init];
//    if (self) {
//        _map = map;
//        _iter = map->begin();
//    }
//    
//    return self;
//}
//
//- (id)nextObject
//{
//    if (_iter == _map->end())
//        return nil;
//    
//    return(_iter++)->first;
//}
//
//@end

@interface HDWMapValueEnumerator : NSEnumerator {
    const ObjectMap *_map;
    ObjectMap::const_iterator _iter;
}

- (id)initWithMap:(const ObjectMap *)map;
@end

@implementation HDWMapValueEnumerator

- (id)initWithMap:(const ObjectMap *)map {
    self = [super init];
    if (self) {
        _map = map;
        _iter = map->begin();
    }
    
    return self;
}

- (id)nextObject
{
    if (_iter == _map->end())
        return nil;
    
    return(_iter++)->second;
}

@end

@interface HDWMap() {
    ObjectMapPtr _map;
}

@end

@implementation HDWMap {
    
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _map = ObjectMapPtr(new ObjectMap());
    }
    return self;
}

- (void)insertObject:(id)object forKey:(KeyType)key {
    _map->insert(std::make_pair(key, object));
}

- (NSEnumerator *)enumerateValues {
    return [[HDWMapValueEnumerator alloc] initWithMap:_map.get()];
}

- (id)serverForExactDate:(KeyType)elem {
    if (_map->empty())
        return nil;
    
    auto iter = _map->find(elem);
    if (iter != _map->end())
        return iter->second;
    
    iter = _map->upper_bound(elem);
    if (iter == _map->begin())
        return nil;
    
    iter--;
    return iter->second;
}

- (id)onlinServerForFutureDate:(KeyType)elem withChunks:(NSDictionary *)serverChunks {
    if (_map->empty())
        return nil;
    
    auto iter = _map->find(elem);
    if (iter != _map->end())
        return iter->second;
    
    iter = _map->upper_bound(elem);
    if (iter != _map->begin())
        iter--;
    
    std::map<int64_t, HDWCameraServerItem *> sortedItems;
    while (iter != _map->end()) {
        HDWCameraServerItem *item = iter->second;
        HDWChunks *chunks = serverChunks[item.server.serverId];
        if (chunks && item.server.status.intValue != Status_Offline) {
            int64_t chunkStart = [chunks chunkStartForLaterDateMsecs:elem];
            if (chunkStart != -1) {
                sortedItems[chunkStart] = item;
            }
        }
        
        iter++;
    }
    
    if (!sortedItems.empty())
        return sortedItems.begin()->second;
    else
        return nil;
}

@end
