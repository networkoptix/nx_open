//
//  HDWQueue.h
//  HDWitness
//
//  Created by Ivan Vigasin on 5/1/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWQueue : NSObject<NSCopying>

- (void)addObject:(id)object;
- (void)addObjects:(NSArray*)objects;

- (id)takeObject;
- (BOOL)isEmpty;

- (id)getObject:(int)index;
- (NSUInteger)count;

- (void)removeAllObjects;
@end
