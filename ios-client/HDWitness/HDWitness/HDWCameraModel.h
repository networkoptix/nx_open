//
//  HDWCameraModel.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWResolution.h"
#import "HDWUUID.h"
#import "HDWOrderedDictionary.h"
#import "HDWModelVisitor.h"

enum CameraStatus {
    Status_Offline,
    Status_Unauthorized,
    Status_Online,
    Status_Recording
};


@interface HDWResourceId : NSObject<NSCopying>

+ (instancetype)resourceIdWithInt:(int)resourceId;
+ (instancetype)resourceIdWithUuid:(HDWUUID *)resourceId;
+ (instancetype)resourceIdWithString:(NSString *)resourceId;

- (instancetype)initWithInt:(int)resourceId;
- (instancetype)initWithUuid:(HDWUUID *)resourceId;
- (instancetype)initWithString:(NSString *)resourceId;

- (NSString *)toString;
- (HDWUUID *)toUUID;

@end

@class HDWECSConfig;
@class HDWServerModel;
@class HDWECSModel;
@class HDWUserModel;


typedef enum : NSInteger {
    VT_MJPEG,
    VT_HLS
} VideoTransport;

typedef enum : NSInteger {
    Q_UNDEFINED = -1,
    Q_LOW,
    Q_MEDIUM,
    Q_HIGH,
    Q_BEST
} HDWQuality;

@interface HDWStreamDescriptor : NSObject

- (id)initWithProtocol:(VideoTransport)transport transcoding:(BOOL)transcoding resolution:(NSString *)resolution andRequestParameter:(NSString *)reqParam;

@property(nonatomic,readonly) VideoTransport transport;
@property(nonatomic,readonly) BOOL transcoding;
@property(nonatomic,readonly) HDWResolution *resolution;
@property(nonatomic,readonly) NSString *requestParameter;
@property(nonatomic) HDWQuality quality;

@end

/**
 * Model representing CameraServerItem (camera history) object
 */
@interface HDWCameraServerItem : NSObject

- (instancetype)initWithServer:(HDWServerModel *)server andTimestamp:(int64_t)timestamp;

@property(nonatomic, readonly) int64_t timestamp;
@property(nonatomic, weak, readonly) HDWServerModel *server;

@end

@interface HDWChunks : NSObject

- (instancetype)initWithArray:(NSArray *)array;
- (BOOL)hasChunkMatchingDateMsecs:(int64_t)msecs;
- (int64_t)chunkStartForLaterDateMsecs:(int64_t)msecs;

@end

/**
 * Model representing Camera object
 */
@interface HDWCameraModel : NSObject<NSCopying>

@property(nonatomic, readonly) HDWResourceId *cameraId;
@property(nonatomic) NSString *name;
@property(nonatomic, readonly) NSString *physicalId;
@property(nonatomic) NSString *secondaryStreamQuality;
@property(nonatomic) NSNumber *status;
@property(nonatomic) BOOL disabled;
@property(nonatomic) NSArray *streamDescriptors;
@property(nonatomic, readonly) NSNumber *angle;
@property(nonatomic, readonly) NSNumber *aspectRatio;
@property(nonatomic, readonly) BOOL onlineOrRecording;
@property(nonatomic,readonly) NSMutableDictionary *serverChunks;
@property(nonatomic, weak, readonly) HDWServerModel *server;

- (HDWCameraModel*)initWithDict:(NSDictionary *) dict andServer:(HDWServerModel *)server;

- (id)copyWithZone:(NSZone *)zone;

- (NSNumber *)calculatedStatus;

- (BOOL)hasChunks;
- (BOOL)hasChunkForExactDate:(NSTimeInterval)date;
- (BOOL)hasChunkForLaterDate:(NSTimeInterval)date;
- (void)setChunks:(HDWChunks *)chunks forServer:(HDWServerModel *)server;

- (BOOL)isEqualToCamera:(HDWCameraModel*)camera;

- (void)addCameraServerItem:(HDWCameraServerItem *)item;
- (HDWServerModel *)onlinServerForFutureDate:(NSTimeInterval)date withChunks:(NSDictionary *)serverChunks;

- (NSComparisonResult)compare:(HDWCameraModel*)camera;
@end


/**
 * Model representing Server object. Cameras are inside.
 */
@interface HDWServerModel : NSObject<NSCopying>

@property(nonatomic) HDWOrderedDictionary *cameras;
@property(nonatomic) NSNumber *status;
@property(nonatomic) NSUInteger cameraCount;
@property(nonatomic) HDWResourceId *serverId;
@property(nonatomic) NSString *name;
@property(nonatomic) HDWResourceId *guid;
@property(nonatomic) NSURL *streamingBaseUrl;
@property(nonatomic, weak) HDWECSModel* ecs;
@property(nonatomic) NSArray *netAddrList;

- (HDWServerModel *)initWithDict:(NSDictionary *)dict andECS:(HDWECSModel *) ecs;

- (HDWCameraModel *)cameraAtIndex:(NSUInteger)index;
- (HDWCameraModel *)findCameraById:(HDWResourceId *)cameraId;
- (HDWCameraModel *)findCameraByPhysicalId:(NSString *)physicalId;

- (void)setStatus:(NSNumber *) newStatus;
- (void)insertCamera:(HDWCameraModel *)camera atPosition:(NSInteger)position;
- (void)addOrReplaceCamera:(HDWCameraModel *) camera;

- (void)removeAllCameras;
- (void)removeCameraById:(HDWResourceId *) cameraId;
- (NSUInteger)indexOfCameraWithId:(HDWResourceId *) serverId;

- (BOOL)isEqualToServer:(HDWServerModel*)server;
- (NSComparisonResult)compare:(HDWServerModel*)server;
@end


@interface HDWLayoutModel : NSObject

@property(nonatomic,readonly) HDWOrderedDictionary *items;
@property(nonatomic,readonly) HDWResourceId *layoutId;
@property(nonatomic,readonly) HDWResourceId *userid;
@property(nonatomic,readonly) NSString *name;
@property(nonatomic,readonly,weak) HDWUserModel* user;

- (BOOL)hasCameraWithPhysicalId:(NSString *)physicalId;
- (HDWLayoutModel *)initWithDict:(NSDictionary *)dict andUser:(HDWUserModel *) user findInCameras:(NSDictionary*)camerasById;
@end


@interface HDWUserModel : NSObject

@property(nonatomic,readonly) HDWOrderedDictionary *layouts;
@property(nonatomic,readonly) HDWResourceId *userId;
@property(nonatomic,readonly) NSString *name;
@property(nonatomic,readonly) BOOL isAdmin;
@property(nonatomic,readonly) BOOL isArchiveAccessible;
@property(nonatomic,readonly) uint64_t rights;
@property(nonatomic,readonly,weak) HDWECSModel* ecs;

- (HDWUserModel *)initWithDict:(NSDictionary *)dict andECS:(HDWECSModel *)ecs;
- (void)addOrReplaceLayout:(HDWLayoutModel *)layout;
- (BOOL)hasCameraWithPhysicalId:(NSString *)physicalId;
@end


/**
 * Model representing list of servers.
 */
@interface HDWECSModel : NSObject<NSCopying> {
}

@property(nonatomic,readonly) HDWECSConfig *config;

@property(nonatomic,readonly) HDWOrderedDictionary *servers;
@property(nonatomic,readonly) HDWOrderedDictionary *users;

@property(nonatomic) NSMutableDictionary *camerasById;

- (instancetype)initWithECSConfig:(HDWECSConfig *)newConfig;

- (void) clear;
- (void)addServers:(NSArray *)servers;
- (void)addServer:(HDWServerModel *)server;
- (void)insertServer:(HDWServerModel *)server atPosition:(NSInteger)position;
- (NSUInteger)addOrUpdateServer:(HDWServerModel *)server;

- (void)updateServer:(HDWServerModel *)server;
- (void)insertCamera:(HDWCameraModel *)camera atIndexPath:(NSIndexPath *)indexPath;
- (void)addOrReplaceCamera:(HDWCameraModel *)camera;
- (void)mergeCameraFromDict:(NSDictionary *)cameraDict;

- (HDWServerModel *)findServerById:(HDWResourceId *)id;
- (HDWServerModel *)findServerByGuid:(HDWResourceId *)guid;
- (HDWCameraModel *)findCameraById:(HDWResourceId *)cameraId;
- (HDWCameraModel *)findCameraByPhysicalId:(NSString *)physicalId;
- (HDWCameraModel *)findCameraById:(HDWResourceId *)id atServer:(HDWResourceId *)serverId;
- (HDWUserModel *)findUserById:(HDWResourceId *)userId;

- (HDWServerModel *) serverAtIndex:(NSInteger)index;
- (NSUInteger) indexOfServerWithId:(HDWResourceId *)serverId;

- (HDWCameraModel *)cameraAtIndexPath:(NSIndexPath *)indexPath;
- (NSIndexPath *)indexPathOfCameraWithId:(HDWResourceId *)cameraId;
- (NSIndexPath *)indexPathOfCameraWithId:(HDWResourceId *)cameraId andServerId:(HDWResourceId *)serverId;

- (void)addUsers:(NSArray *)users;
- (void)addUser:(HDWUserModel *)user;
- (BOOL)isCamera:(HDWCameraModel *)camera accessibleToUser:(NSString *)username;
- (BOOL)isCameraArchive:(HDWCameraModel *)camera accessibleToUser:(NSString *)username;

- (void)removeAllServers;
- (void)removeServerById:(HDWResourceId *)serverId;
- (void)removeCameraById:(HDWResourceId *)cameraId;
- (void)removeCameraById:(HDWResourceId *)cameraId andServerId:(HDWResourceId *)serverId;

- (NSUInteger)count;

- (id)copyWithZone:(NSZone *)zone;
@end
