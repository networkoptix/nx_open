//
//  HDWCameraModel.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

/**
 * Model representing Server object. Cameras are inside.
 */
@interface HDWServerModel : NSObject {
}

@property NSNumber *serverId;
@property NSString *name;
@property NSURL *streamingUrl;
@property NSMutableArray *cameras;
@end

/**
 * Model representing Camera object
 */
@interface HDWCameraModel : NSObject {
}

@property NSNumber *cameraId;
@property NSNumber *serverId;
@property NSString *name;
@property NSString *physicalId;
@property NSNumber *status;

@property NSURL *videoUrl;
@property NSURL *thumbnailUrl;


@end

/**
 * Model representing list of servers.
 */
@interface HDWServersModel : NSObject {
    NSMutableArray *servers;
}

-(void) addServers: (NSArray*) servers;
-(void) addServer: (HDWServerModel*) server;

-(void) updateServer: (HDWServerModel*) server;
-(void) updateCamera: (HDWCameraModel*) camera;

-(HDWServerModel*) findServerById: (NSNumber*) id;
-(HDWCameraModel*) findCameraById: (NSNumber*) id atServer: (NSNumber*) serverId;

-(HDWServerModel*) getServerAtIndex: (NSInteger) index;
-(HDWCameraModel*) getCameraForIndexPath: (NSIndexPath*) indexPath;
-(NSIndexPath*) getIndexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId;
-(int) count;

@end