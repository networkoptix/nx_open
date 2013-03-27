//
//  HDWCameraModel.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@class HDWServerModel;

/**
 * Model representing Camera object
 */
@interface HDWCameraModel : NSObject {
}

@property NSNumber *cameraId;
@property HDWServerModel *server;
@property NSString *name;
@property NSString *physicalId;
@property NSNumber *status;

@property(readonly) NSURL *videoUrl;
@property(readonly) NSURL *thumbnailUrl;


@end

/**
 * Model representing Server object. Cameras are inside.
 */
@interface HDWServerModel : NSObject {
}

@property NSNumber *serverId;
@property NSString *name;
@property NSURL *streamingUrl;
@property NSMutableArray *cameras;

-(void) addOrUpdateCamera: (HDWCameraModel*) camera;

-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId;
-(void) removeCameraById: (NSNumber*) cameraId;

@end

/**
 * Model representing list of servers.
 */
@interface HDWServersModel : NSObject {
    NSMutableArray *servers;
}

-(void) addServers: (NSArray*) servers;
-(void) addServer: (HDWServerModel*) server;
-(void) addOrUpdateServer: (HDWServerModel*) server;

-(void) updateServer: (HDWServerModel*) server;
-(void) addOrUpdateCamera: (HDWCameraModel*) camera;

-(HDWServerModel*) findServerById: (NSNumber*) id;
-(HDWCameraModel*) findCameraById: (NSNumber*) id atServer: (NSNumber*) serverId;

-(HDWServerModel*) getServerAtIndex: (NSInteger) index;
-(NSUInteger) getIndexOfServerWithId: (NSNumber*) serverId;

-(HDWCameraModel*) getCameraForIndexPath: (NSIndexPath*) indexPath;
-(NSIndexPath*) getIndexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId;

-(void) removeServerById: (NSNumber*) serverId;
-(void) removeCameraById: (NSNumber*) cameraId;

-(int) count;

@end