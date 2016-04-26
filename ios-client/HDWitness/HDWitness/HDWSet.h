//
//  HDWSet.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWSet : NSObject

- (instancetype)init;
- (instancetype)initWithComparator:(NSComparator)comparator;

- (void)insert:(id)object;
- (NSEnumerator *)enumerate;

- (id)lowerBound:(id)elem;
- (id)upperBound:(id)elem;

@end
