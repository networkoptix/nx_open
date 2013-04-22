//
//  HDWCameraModel.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

enum CameraStatus {
    Status_Offline,
    Status_Unauthorized,
    Status_Online,
    Status_Recording
};


@interface HDWECSConfig : NSObject<NSCopying> {
}

@property NSString *name;
@property NSString *host;
@property NSString *port;
@property NSString *login;
@property NSString *password;

@property (readonly) NSURL *url;

+ (HDWECSConfig*) defaultConfig;

- (id)copyWithZone:(NSZone *)zone;
- (BOOL)isFilled;

@end

@class HDWServerModel;
@class HDWECSModel;

/**
 * Model representing Camera object
 */
@interface HDWCameraModel : NSObject {
}

@property(readonly) NSNumber *cameraId;
@property(readonly) NSString *name;
@property(readonly) NSString *physicalId;
@property(readonly) NSNumber *status;
@property(readonly) BOOL disabled;

@property(readonly) HDWServerModel *server;

-(HDWCameraModel*) initWithDict: (NSDictionary*) dict andServer: (HDWServerModel*) server;
-(void) setStatus: (NSNumber*) newStatus;
-(void) setDisabled: (BOOL) newDisabled;
-(NSURL *) liveUrl;
-(NSURL *) archiveUrlForDate: (NSDate*)date;
-(NSURL *) thumbnailUrl;

@end

/**
 * Model representing Server object. Cameras are inside.
 */
@interface HDWServerModel : NSObject {
    NSMutableDictionary *cameras;
}

@property(readonly) NSNumber *status;
@property(readonly) NSUInteger cameraCount;
@property(readonly) NSNumber *serverId;
@property(readonly) NSString *name;
@property(readonly) NSURL *streamingUrl;
@property(readonly) HDWECSModel* ecs;

-(HDWServerModel*) initWithDict: (NSDictionary*) dict andECS: (HDWECSModel*) ecs;

-(HDWCameraModel*) cameraAtIndex:(NSUInteger)index;
-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId;

-(void) setStatus: (NSNumber*) newStatus;
-(void) addOrReplaceCamera: (HDWCameraModel*) camera;
-(void) removeCameraById: (NSNumber*) cameraId;
-(NSUInteger) indexOfCameraWithId: (NSNumber*) serverId;
-(NSArray*) enabledCamerasIds;
-(NSArray*) enabledCameras;

@end

/**
 * Model representing list of servers.
 */
@interface HDWECSModel : NSObject {
    HDWECSConfig* ecsConfig;
    NSMutableDictionary *servers;
}

@property(readonly) HDWECSConfig* config;

-(id)initWithECSConfig: (HDWECSConfig*)newConfig;

-(void) addServers: (NSArray*) servers;
-(void) addServer: (HDWServerModel*) server;
-(NSUInteger) addOrUpdateServer: (HDWServerModel*) server;

-(void) updateServer: (HDWServerModel*) server;
-(NSIndexPath*) addOrReplaceCamera: (HDWCameraModel*) camera;

-(HDWServerModel*) findServerById: (NSNumber*) id;
-(HDWCameraModel*) findCameraById: (NSNumber*) id atServer: (NSNumber*) serverId;

-(HDWServerModel*) serverAtIndex: (NSInteger) index;
-(NSUInteger) indexOfServerWithId: (NSNumber*) serverId;

-(HDWCameraModel*) cameraAtIndexPath: (NSIndexPath*) indexPath;
-(NSIndexPath*) indexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId;

-(void) removeServerById: (NSNumber*) serverId;
-(void) removeCameraById: (NSNumber*) cameraId andServerId: (NSNumber*) serverId;

-(int) count;

@end