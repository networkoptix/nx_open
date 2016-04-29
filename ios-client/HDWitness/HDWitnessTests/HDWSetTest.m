//
//  HDWSetTest.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/31/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "HDWSet.h"

@interface HDWSetTest : XCTestCase

@end

@implementation HDWSetTest

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testDefaultComparator {
    HDWSet *set = [[HDWSet alloc] init];
    
    [set insert:@"C"];
    [set insert:@"B"];
    [set insert:@"A"];
    [set insert:@"A"];
    [set insert:@"c"];
    [set insert:@"c"];
    [set insert:@"b"];
    [set insert:@"a"];
    
    NSEnumerator *enumerator = [set enumerate];

    XCTAssert([[enumerator nextObject] isEqual:@"A"]);
    XCTAssert([[enumerator nextObject] isEqual:@"B"]);
    XCTAssert([[enumerator nextObject] isEqual:@"C"]);
    XCTAssert([[enumerator nextObject] isEqual:@"a"]);
    XCTAssert([[enumerator nextObject] isEqual:@"b"]);
    XCTAssert([[enumerator nextObject] isEqual:@"c"]);
    XCTAssert([enumerator nextObject] == nil);
}

- (void)testCustomComparator {
    HDWSet *set = [[HDWSet alloc] initWithComparator:^NSComparisonResult(NSString *s1, NSString *s2) {
        return [s2.lowercaseString compare:s1.lowercaseString];
    }];
    
    [set insert:@"C"];
    [set insert:@"B"];
    [set insert:@"A"];
    [set insert:@"c"];
    [set insert:@"b"];
    [set insert:@"a"];

    NSEnumerator *enumerator = [set enumerate];
    
    XCTAssert([[enumerator nextObject] isEqual:@"C"]);
    XCTAssert([[enumerator nextObject] isEqual:@"B"]);
    XCTAssert([[enumerator nextObject] isEqual:@"A"]);
    XCTAssert([enumerator nextObject] == nil);
}

- (void)testBounds {
    HDWSet *set = [[HDWSet alloc] init];

    [set insert:@"C"];
    [set insert:@"A"];
    
    XCTAssert([[set lowerBound:@"B"] isEqual:@"C"]);
    XCTAssert([[set upperBound:@"B"] isEqual:@"C"]);

    [set insert:@"B"];
    XCTAssert([[set lowerBound:@"B"] isEqual:@"B"]);
    XCTAssert([[set upperBound:@"B"] isEqual:@"C"]);
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
