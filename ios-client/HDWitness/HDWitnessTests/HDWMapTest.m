//
//  HDWMapTest.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "HDWMap.h"

@interface Penis : NSObject

@end

@implementation Penis

-(void)dealloc {
    NSLog(@"Dealloc");
}
@end

@interface HDWMapTest : XCTestCase

@end

@implementation HDWMapTest

- (void)mapUp {
    [super setUp];
    // Put mapup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testDefaultComparator {
    HDWMap *map = [[HDWMap alloc] init];
    
    [map insertObject:@"C" forKey:@3];
    [map insertObject:@"B" forKey:@2];
    [map insertObject:@"A" forKey:@1];
    [map insertObject:@"A" forKey:@1];
    [map insertObject:@"c" forKey:@3];
    [map insertObject:@"c" forKey:@3];
    [map insertObject:@"b" forKey:@2];
    [map insertObject:@"a" forKey:@1];
    
    NSEnumerator *enumerator = [map enumerateValues];
    
    XCTAssert([[enumerator nextObject] isEqual:@"A"]);
    XCTAssert([[enumerator nextObject] isEqual:@"B"]);
    XCTAssert([[enumerator nextObject] isEqual:@"C"]);
    XCTAssert([enumerator nextObject] == nil);
}

- (void)testBounds {
    HDWMap *map = [[HDWMap alloc] init];
    
    [map insertObject:@"C" forKey:@3];
    [map insertObject:@"A" forKey:@1];
    
    XCTAssert([[map preElementValue:@2] isEqual:@"A"]);
    XCTAssert([map preElementValue:@0] == nil);
    XCTAssert([[map preElementValue:@4] isEqual:@"C"]);
    
    [map insertObject:@"B" forKey:@2];
    
    XCTAssert([[map preElementValue:@2] isEqual:@"B"]);
    XCTAssert([map preElementValue:@0] == nil);
    XCTAssert([[map preElementValue:@4] isEqual:@"C"]);
}

- (void)helper {
    HDWMap *map = [[HDWMap alloc] init];
    [map insertObject:[[Penis alloc] init] forKey:@1];
}

- (void)testRelease {
    [self helper];
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
