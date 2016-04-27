//
//  HDWSet.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWSet.h"

#include <set>
#include <memory>

namespace  {
    typedef std::function<bool(id,id)> Less;
    
    struct ObjectLess {
        bool operator()(id obj1, id obj2) {
            return [obj1 compare:obj2] == NSOrderedAscending;
        }
    };
    
    struct CustomLess {
        NSComparator _comparator;
        
        CustomLess(NSComparator comparator) {
            _comparator = comparator;
        }
        
        bool operator()(id obj1, id obj2) {
            return _comparator(obj1, obj2) == NSOrderedAscending;
        }
    };
    
    typedef std::set<NSObject *, Less> ObjectSet;
    typedef std::unique_ptr<ObjectSet> ObjectSetPtr;
}

@interface HDWSetEnumerator : NSEnumerator {
    const ObjectSet *_set;
    ObjectSet::const_iterator _iter;
}

- (id)initWithSet:(const ObjectSet *)set;
@end

@implementation HDWSetEnumerator

- (id)initWithSet:(const ObjectSet *)set {
    self = [super init];
    if (self)
    {
        _set = set;
        _iter = set->begin();
    }
    
    return self;
}

- (id)nextObject
{
    if (_iter == _set->end())
        return nil;
    
    return *(_iter++);
}

@end

@interface HDWSet() {
    ObjectSetPtr _set;
}

@end

@implementation HDWSet {
    
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _set = ObjectSetPtr(new ObjectSet(ObjectLess()));
    }
    return self;
}

- (instancetype)initWithComparator:(NSComparator)comparator {
    self = [self init];
    if (self) {
        CustomLess customLess(comparator);
        _set = ObjectSetPtr(new ObjectSet(customLess));
    }
    
    return self;
}

- (void)insert:(id)object {
    _set->insert(object);
}

- (NSEnumerator *)enumerate {
    return [[HDWSetEnumerator alloc] initWithSet:_set.get()];
}

- (id)lowerBound:(id)elem {
    const auto& iter = _set->lower_bound(elem);
    return (iter != _set->end()) ? *iter : nil;
}

- (id)upperBound:(id)elem {
    const auto& iter = _set->upper_bound(elem);
    return (iter != _set->end()) ? *iter : nil;
}

@end
