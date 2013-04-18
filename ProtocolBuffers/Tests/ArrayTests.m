// Protocol Buffers for Objective C
//
// Copyright 2010 Booyah Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "ArrayTests.h"

#import "PBArray.h"

@implementation ArrayTests

#pragma mark PBArray

- (void)testCount
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertEquals([array count], (NSUInteger)3, nil);
	STAssertEquals(array.count, (NSUInteger)3, nil);
}

- (void)testValueType
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertEquals(array.valueType, PBArrayValueTypeInt32, nil);
}

- (void)testPrimitiveAccess
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertEquals([array int32AtIndex:1], 2, nil);
}


- (void)testEmpty
{
	PBArray *array = [[PBArray alloc] init];
	STAssertEquals(array.count, (NSUInteger)0, nil);
	STAssertEquals(array.valueType, PBArrayValueTypeBool, nil);
	STAssertEquals(array.data, (const void *)nil, nil);
	STAssertThrowsSpecificNamed([array boolAtIndex:0], NSException, NSRangeException, nil);
}
/*
- (void)testCopy
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *original = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	PBArray *copy = [original copy];
	STAssertEquals(original.valueType, copy.valueType, nil);
	STAssertEquals(original.count, copy.count, nil);
	STAssertEquals([original int32AtIndex:1], [copy int32AtIndex:1], nil);
}
*/

- (void)testArrayAppendingArray
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *a = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	PBArray *b = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];

	PBArray *copy = [a arrayByAppendingArray:b] ;
	STAssertEquals(copy.valueType, a.valueType, nil);
	STAssertEquals(copy.count, a.count + b.count, nil);

}

- (void)testAppendArrayTypeException
{
	const int32_t kValuesA[3] = { 1, 2, 3 };
	const int64_t kValuesB[3] = { 1, 2, 3 };
	PBArray *a = [[PBArray alloc] initWithValues:kValuesA count:3 valueType:PBArrayValueTypeInt32];
	PBArray *b = [[PBArray alloc] initWithValues:kValuesB count:3 valueType:PBArrayValueTypeInt64];
	STAssertThrowsSpecificNamed([a arrayByAppendingArray:b], NSException, PBArrayTypeMismatchException, nil);
}

- (void)testRangeException
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertThrowsSpecificNamed([array int32AtIndex:10], NSException, NSRangeException, nil);
}

- (void)testTypeMismatchException
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertThrowsSpecificNamed([array boolAtIndex:0], NSException, PBArrayTypeMismatchException, nil);
}

- (void)testNumberExpectedException
{
	NSArray *objects = [[NSArray alloc] initWithObjects:@"Test", nil];
	STAssertThrowsSpecificNamed([[PBArray alloc] initWithArray:objects valueType:PBArrayValueTypeInt32],
								NSException, PBArrayNumberExpectedException, nil);
}

#pragma mark PBAppendableArray
/*
- (void)testAddValue
{
	PBAppendableArray *array = [[PBAppendableArray alloc] initWithValueType:PBArrayValueTypeInt32];
	[array addInt32:1];
	[array addInt32:4];
	STAssertEquals(array.count, (NSUInteger)2, nil);
	STAssertThrowsSpecificNamed([array addBool:NO], NSException, PBArrayTypeMismatchException, nil);
}



- (void)testAppendArray
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *source = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	PBAppendableArray *array = [[PBAppendableArray alloc] initWithValueType:PBArrayValueTypeInt32];
	[array appendArray:source];
	STAssertEquals(array.count, source.count, nil);
	STAssertEquals([array int32AtIndex:1], 2, nil);
}
*/
- (void)testAppendValues
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBAppendableArray *array = [[PBAppendableArray alloc] initWithValueType:PBArrayValueTypeInt32];
	[array appendValues:kValues count:3];
	STAssertEquals(array.count, (NSUInteger)3, nil);
	STAssertEquals([array int32AtIndex:1], 2, nil);
}

- (void)testEqualValues
{
	const int32_t kValues[3] = { 1, 2, 3 };
	PBArray *array1 = [[PBArray alloc] initWithValues:kValues count:2 valueType:PBArrayValueTypeInt32];

	// Test self equality.
	STAssertEqualObjects(array1, array1, nil);

	PBArray *array2 = [[PBArray alloc] initWithValues:kValues count:2 valueType:PBArrayValueTypeInt32];
	// Test other equality.
	STAssertEqualObjects(array1, array2, nil);

	// Test non equality of nil.
	STAssertFalse([array1 isEqual:nil], nil);

	// Test non equality of other object type.
	STAssertFalse([array1 isEqual:@""], nil);

	// Test non equality of arrays of different sizes with same prefix.
	PBArray *array3 = [[PBArray alloc] initWithValues:kValues count:3 valueType:PBArrayValueTypeInt32];
	STAssertFalse([array1 isEqual:array3], nil);

	// Test non equality of arrays of same sizes with different contents.
	const int32_t kValues2[2] = { 2, 1 };
	PBArray *array4 = [[PBArray alloc] initWithValues:kValues2 count:2 valueType:PBArrayValueTypeInt32];
	STAssertFalse([array1 isEqual:array4], nil);

}

@end