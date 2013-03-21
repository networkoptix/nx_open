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
@end

/**
 * Model representing list of servers.
 */
@interface HDWServersModel : NSObject {
    NSMutableArray *servers;
}

-(void) addServers: (NSArray*) servers;
-(void) addServer: (HDWServerModel*) server;
-(HDWServerModel*) findServerById: (NSNumber*) id;
-(HDWServerModel*) getServerAtIndex: (NSInteger) index;
-(HDWCameraModel*) getCameraForIndexPath: (NSIndexPath*) indexPath;
-(int) count;

@end