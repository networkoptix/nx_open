//
//  HDWProtocolFactory.h
//  HDWitness
//
//  Created by Ivan Vigasin on 3/26/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@class HDWECSModel;
@class HDWApiConnectInfo;
@class HDWCameraModel;
@class HDWServerModel;
@class HDWStreamDescriptor;
@class HDWECSConfig;

@protocol HDWProtocol <NSObject>
- (Class)eventSourceClass;
- (NSString *)eventsPath;
- (BOOL)processMessage:(NSData *)message withECS:(HDWECSModel *)ecs;
+ (HDWApiConnectInfo *)parseConnectInfo:(NSData *)data;

- (BOOL)useBasicAuth;
- (NSURL *)chunksUrlForCamera:(HDWCameraModel *)camera atServer:(HDWServerModel *)server;
- (NSURL *)liveUrlForCamera:(HDWCameraModel *)camera andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor;
- (NSURL *)archiveUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor;
- (NSURL *)thumbnailUrlForCamera:(HDWCameraModel *)camera;
- (NSURL *)urlForConfig:(HDWECSConfig *)config;

- (NSString *)systemTimePath;
@end

@interface HDWProtocolFactory : NSObject
+ (HDWApiConnectInfo *)parseConnectInfo:(NSData *)data andDetectProtocol:(id<HDWProtocol> *)protocol;
@end
