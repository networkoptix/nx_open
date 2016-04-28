//
//  HDWQueue.m
//  HDWitness
//
//  Created by Ivan Vigasin on 5/1/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWQueue.h"

@interface HDWQueue()

@property(nonatomic) NSMutableArray* objects;

@end

@implementation HDWQueue

- (instancetype)init {
    self = [super init];
    if (self) {
        _objects = [[NSMutableArray alloc] init];
    }
    
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    HDWQueue *queue = [[HDWQueue alloc] init];
    queue.objects = [self.objects mutableCopyWithZone:zone];
    return queue;
}

- (void)addObject:(id)object {
    [_objects addObject:object];
}

- (void)addObjects:(NSArray *)objects {
    for (id object in objects) {
        [self addObject:object];
    }
}

- (id)takeObject {
    id object = nil;
    if ([_objects count] > 0) {
        object = _objects[0];
        [_objects removeObjectAtIndex:0];
    }
    return object;
}

- (id)getObject:(int)index {
    return _objects[index];
}

- (NSUInteger)count {
    return _objects.count;
}

- (void)removeAllObjects {
    [_objects removeAllObjects];
}

- (BOOL)isEmpty {
    return _objects.count == 0;
}

@end
