//
//  HDWCameraModel.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWCameraModel.h"

@implementation HDWCameraModel

-(NSURL*) videoUrl {
    return [NSURL URLWithString:[NSString stringWithFormat:@"/media/%@.mpjpeg", _physicalId] relativeToURL:_server.streamingUrl];
}

-(NSURL*) thumbnailUrl {
    return [NSURL URLWithString:[NSString stringWithFormat:@"/api/image?physicalId=%@&time=now", _physicalId] relativeToURL:_server.streamingUrl];
}

@end

@implementation HDWServerModel

-(id)init {
    self = [super init];
    if (self) {
        _cameras = [[NSMutableArray alloc] init];
    }
    
    return self;
}

-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId {
    for (HDWCameraModel* camera in _cameras) {
        if ([camera.cameraId isEqualToNumber:cameraId])
            return camera;
    }
    
    return nil;
}

-(void) removeCameraById: (NSNumber*) cameraId {
    HDWCameraModel *camera = [self findCameraById:cameraId];
    [_cameras removeObject:camera];
}

-(void) addOrUpdateCamera: (HDWCameraModel*) newCamera {
    NSIndexSet *camerasIndexes = [_cameras indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
        HDWCameraModel *camera = (HDWCameraModel*) obj;
        if (camera.cameraId == newCamera.cameraId) {
            *stop = YES;
            return YES;
        } else {
            return NO;
        }
    }];
    
    if (camerasIndexes.count == 1) {
        [_cameras replaceObjectAtIndex:camerasIndexes.firstIndex withObject:newCamera];
    } else {
        [_cameras addObject:newCamera];
    }
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

-(void) addOrUpdateServer: (HDWServerModel*) newServer {
    NSIndexSet *serverIndexes = [servers indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
        HDWServerModel *server = (HDWServerModel*) obj;
        if (server.serverId == newServer.serverId) {
            *stop = YES;
            return YES;
        } else {
            return NO;
        }
    }];
    
    if (serverIndexes.count == 1) {
        [servers replaceObjectAtIndex:serverIndexes.firstIndex withObject:newServer];
    } else {
        [servers addObject:newServer];
    }
}

-(void) addOrUpdateCamera: (HDWCameraModel*) camera {
    [camera.server addOrUpdateCamera:camera];
}

-(HDWServerModel*) findServerById: (NSNumber*) id {
    for (HDWServerModel* server in servers) {
        if ([server.serverId isEqualToNumber:id])
            return server;
    }
    
    return nil;
}

-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId atServer: (NSNumber*) serverId {
    HDWServerModel *server = [self findServerById:serverId];
    if (server == nil)
        return nil;
    
    return [server findCameraById:cameraId];
}

-(HDWServerModel*) getServerAtIndex: (NSInteger) index {
    return [servers objectAtIndex: index];
}

-(HDWCameraModel*) getCameraForIndexPath: (NSIndexPath*) indexPath {
    HDWServerModel *server = [servers objectAtIndex: indexPath.section];
    return [server.cameras objectAtIndex: indexPath.row];
}

-(NSUInteger) getIndexOfServerWithId: (NSNumber*) serverId {
    NSUInteger n = 0;
    for (HDWServerModel* server in servers) {
        if ([server.serverId isEqualToNumber:serverId])
            return n;
        
        n++;
    }
    
    return nil;
}

-(NSIndexPath*) getIndexPathOfServerWithId: (NSNumber*) serverId {
    NSInteger section = -1;
    
    NSInteger n = 0;
    for (HDWServerModel* server in servers) {
        if ([server.serverId isEqualToNumber:serverId]) {
            section = n;
            break;
        }
    }
    
    return [NSIndexPath indexPathWithIndex:section];
}

-(NSIndexPath*) getIndexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId {
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

-(void) removeServerById: (NSNumber*) serverId {
    [servers removeObject:serverId];
}

-(void) removeCameraById: (NSNumber*) cameraId andServerId: (NSNumber*) serverId {
    HDWCameraModel *camera = [self findCameraById:cameraId atServer:serverId];
    [camera.server removeCameraById:cameraId];
}

-(int) count {
    return servers.count;
}

@end
