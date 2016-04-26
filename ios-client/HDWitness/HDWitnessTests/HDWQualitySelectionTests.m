//
//  HDWQualitySelectionTests.m
//  HDWitness
//
//  Created by Ivan Vigasin on 07/09/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <XCTest/XCTest.h>

#import "HDWCameraModel.h"
#import "HDWQualityHelper.h"

@interface HDWQualitySelectionTests : XCTestCase

@end

@implementation HDWQualitySelectionTests

- (void)setUp
{
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)verifyResOfDescriptors:(NSArray *)descriptors with:(NSArray *)resolutions {
    XCTAssertEqual(descriptors.count, resolutions.count, @"There should be %ld resolutions", resolutions.count);
    
    NSUInteger length = descriptors.count;
    for (NSUInteger i = 0; i < length; ++i) {
        HDWStreamDescriptor *descriptor = descriptors[i];
        HDWResolution *resolution = descriptor.resolution;
        NSNumber *imageHeight = resolutions[i];
        
        XCTAssertEqual(resolution.height, imageHeight.intValue, @"Resolutions[%ld] should be %ld, not %ld", i, imageHeight.intValue, resolution.height);
    }
    
}

- (void)test5105DN
{
    NSArray *input = @[
                [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"2592x1944" andRequestParameter:@""],
                [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1296x972" andRequestParameter:@""],
                [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@972, @720, @480, @240]];
}

- (void)test4Castor3
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"0x0" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @240]];
}

- (void)testVK21080VFD3V9F
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1920x1080" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"352x288" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @288]];
}

- (void)test3105
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"2048x1536" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1024x768" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @768, @480, @240]];
}

- (void)test2255AMH
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1920x1088" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"960x544" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @544, @240]];
}

- (void)testH18A5CDN4620E
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1280x720" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"320x192" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@720, @480, @192]];
}

- (void)testH18A4RL01
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1280x720" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"640x360" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@720, @480, @360]];
}

- (void)testIPCLR01
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1280x960" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"320x240" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@960, @720, @480, @240]];
}

- (void)testFS_IP3000_M
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"2048x1536" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"640x480" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @240]];
}

- (void)testAXISQ7404Channel4
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"720x576" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"352x288" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@576, @288]];
}

- (void)testH18A2CDN4620E
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"0x0" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"640x480" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@480, @240]];
}

- (void)testVK2_2MPCCPL
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1600x1200" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"320x240" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @240]];
}

- (void)testISDcam
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1920x1080" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"640x360" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @360]];
    
}

- (void)testDWC_MD421D
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1920x1080" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"480x272" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @272]];
    
}

- (void)testACTi_KCM8111
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1920x1080" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @240]];
}

- (void)test12186DN
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"2048x1536" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @240]];
}

- (void)testACTi_TCM3511
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1280x1024" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"320x240" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1024, @720, @480, @240]];
}

- (void)testDCS_6010L
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"1600x1200" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_HLS transcoding:NO resolution:@"400x300" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1080, @720, @480, @300]];
}

- (void)test1300
{
    NSArray *input = @[
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:NO resolution:@"1280x1024" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:NO resolution:@"640x512" andRequestParameter:@""],
                       [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:@"*" andRequestParameter:@""]];
    
    NSArray *descriptors = [HDWQualityHelper matchStreamDescriptors:input];
    [self verifyResOfDescriptors:descriptors with:@[@1024, @720, @512, @240]];
}

@end
