//
//  HDWCameraModel.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWCameraModel.h"

@implementation HDWCameraModel
@end

@implementation HDWServerModel

-(id)init {
    self = [super init];
    if (self) {
        _cameras = [[NSMutableArray alloc] init];
    }
    
    return self;
}

@end

@implementation HDWServersModel

-(id)init {
    self = [super init];
    if (self) {
        servers = [[NSMutableArray alloc] init];
    }
    
    return self;
}

-(void) addServers: (NSArray*) serversArray {
    for (HDWServerModel* server in serversArray) {
        [self addServer: server];
    }
}

-(void) addServer: (HDWServerModel*) server {
    [servers addObject:server];
}

-(void) updateServer: (HDWServerModel*) server {
    HDWServerModel *existingServer = [self findServerById:server.serverId];
    existingServer.name = server.name;
    existingServer.streamingUrl = server.streamingUrl;

}

-(void) updateCamera: (HDWCameraModel*) camera {
    HDWCameraModel *existingCamera = [self findCameraById:camera.cameraId atServer:camera.serverId];
    existingCamera.name = camera.name;
    existingCamera.physicalId = camera.physicalId;
}

-(HDWServerModel*) findServerById: (NSNumber*) id {
    for (HDWServerModel* server in servers) {
        if ([server.serverId isEqualToNumber:id])
            return server;
    }
    
    return nil;
}

-(HDWCameraModel*) findCameraById: (NSNumber*) id atServer: (NSNumber*) serverId {
    HDWServerModel *server = [self findServerById:serverId];
    
    for (HDWCameraModel* camera in server.cameras) {
        if ([camera.cameraId isEqualToNumber:id])
            return camera;
    }
    
    return nil;
}

-(HDWServerModel*) getServerAtIndex: (NSInteger) index {
    return [servers objectAtIndex: index];
}

-(HDWCameraModel*) getCameraForIndexPath: (NSIndexPath*) indexPath {
    HDWServerModel *server = [servers objectAtIndex: indexPath.section];
    return [server.cameras objectAtIndex: indexPath.row];
}

-(NSIndexPath*) getIndexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId; {
    NSInteger section = -1;
    NSInteger row = -1;
    
    NSInteger n = 0;
    for (HDWServerModel* server in servers) {
        if ([server.serverId isEqualToNumber:serverId]) {
            section = n;

            NSInteger m = 0;
            for (HDWCameraModel* camera in server.cameras) {
                if ([camera.cameraId isEqualToNumber:cameraId]) {
                    row = m;
                    break;
                }
                
                m++;
            }
        }
        
        n++;
    }
    
    return [NSIndexPath indexPathForRow:row inSection:section];
}

-(int) count {
    return servers.count;
}

@end
