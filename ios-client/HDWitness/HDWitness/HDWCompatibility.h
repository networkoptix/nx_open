//
//  HDWCompatibility.h
//  HDWitness
//
//  Created by Ivan Vigasin on 7/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#ifndef HDWitness_HDWCompatibility_h
#define HDWitness_HDWCompatibility_h

@interface HDWCompatibilityItem : NSObject

@property NSString *version;
@property NSString *component;
@property NSString *systemVersion;

+ (HDWCompatibilityItem *)itemForVersion:(NSString*)version ofComponent:(NSString*)component andSystemVersion:(NSString*)systemVersion;

@end

@interface HDWCompatibilityChecker : NSObject

- (HDWCompatibilityChecker*) initLocal;
- (HDWCompatibilityChecker*) initWithItems:(NSArray*)items;

- (NSUInteger)itemCount;
- (BOOL)isCompatibleComponent:(NSString*)comp1 ofVersion:(NSString*)ver1 withComponent:(NSString*)comp2 ofVersion:(NSString*)ver2;

@end

#endif
