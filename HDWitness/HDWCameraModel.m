//
//  HDWCameraModel.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWCameraModel.h"

@implementation HDWCameraModel

-(HDWCameraModel*) initWithDict: (NSDictionary*) dict andServer: (HDWServerModel*) server {
    self = [super init];
    
    if (self) {
        _cameraId = dict[@"id"];
        _name = dict[@"name"];
        _status = dict[@"status"];
        _disabled = ((NSNumber*)dict[@"disabled"]).intValue == 1;
        _physicalId = dict[@"physicalId"];
        _server = server;
    }
    
    return self;
}

-(NSURL*) videoUrl {
    return [NSURL URLWithString:[NSString stringWithFormat:@"/media/%@.mpjpeg?resolution=720p", _physicalId] relativeToURL:_server.streamingUrl];
}

-(NSURL*) thumbnailUrl {
    return [NSURL URLWithString:[NSString stringWithFormat:@"/api/image?physicalId=%@&time=now&width=20", _physicalId] relativeToURL:_server.streamingUrl];
}

@end

@implementation HDWServerModel

-(HDWServerModel*) initWithDict: (NSDictionary*) dict {
    self = [super init];
    
    if (self) {
        _serverId = dict[@"id"];
        _name = dict[@"name"];
        _streamingUrl = [NSURL URLWithString:dict[@"streamingUrl"]];
        
        cameras = [[NSMutableDictionary alloc] init];
    }
    
    return self;
}

-(HDWCameraModel*) cameraAtIndex:(NSUInteger)index {
    return [[self enabledCameras] objectAtIndex:index];
}

-(void) update: (HDWServerModel*) server {
    _name = server.name;
    _streamingUrl = server.streamingUrl;
}

-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId {
    return [cameras objectForKey:cameraId];
}

-(void) removeCameraById: (NSNumber*) cameraId {
    [cameras removeObjectForKey:cameraId];
}

-(void) addOrReplaceCamera: (HDWCameraModel*) camera {
    [cameras setObject:camera forKey:camera.cameraId];
}

-(NSUInteger) cameraCount {
    return [self enabledCamerasIdSet].count;
}

-(NSUInteger) indexOfCameraWithId: (NSNumber*) cameraId {
    return [[self enabledCamerasIds] indexOfObject:cameraId];
}

-(NSSet*) enabledCamerasIdSet {
    return [cameras keysOfEntriesPassingTest:^BOOL(id key, id obj, BOOL *stop) {
        return ((HDWCameraModel*)obj).disabled == NO;
    }];
}

-(NSArray*) enabledCamerasIds {
    NSSet* keySet = [self enabledCamerasIdSet];
    
    NSMutableArray* keyArray = [NSMutableArray array];
    for (id key in cameras.allKeys) {
        if ([keySet containsObject:key]) {
            [keyArray addObject:key];
        }
    }
    
    return keyArray;
}

-(NSArray*) enabledCameras {
    NSSet* keySet = [self enabledCamerasIdSet];
    
    NSMutableArray* valueArray = [NSMutableArray array];
    for (HDWCameraModel* camera in cameras.allValues) {
        if ([keySet containsObject:camera.cameraId]) {
            [valueArray addObject:camera];
        }
    }
    
    return valueArray;
}

@end

@implementation HDWServersModel

-(id)init {
    self = [super init];
    if (self) {
        servers = [[NSMutableDictionary alloc] init];
    }
    
    return self;
}

-(void) addServers: (NSArray*) serversArray {
    for (HDWServerModel* server in serversArray) {
        [self addServer: server];
    }
}

-(void) addServer: (HDWServerModel*) server {
    [servers setObject:server forKey:server.serverId];
}

-(NSUInteger) addOrUpdateServer: (HDWServerModel*) newServer {
    HDWServerModel *server = [servers objectForKey:newServer.serverId];
    if (server) {
        [server update:newServer];
        return NSNotFound;
    } else {
        [servers setObject:newServer forKey:newServer.serverId];
        return [self indexOfServerWithId:newServer.serverId];
    }
}

-(void) updateServer: (HDWServerModel*) server {
    HDWServerModel *existingServer = [self findServerById:server.serverId];
    [existingServer update:server];
}

-(NSIndexPath*) addOrReplaceCamera: (HDWCameraModel*) camera {
    BOOL insert = NO;
    
    if ([self findCameraById:camera.cameraId atServer:camera.server.serverId] == nil) {
        insert = YES;
    }
    
    [camera.server addOrReplaceCamera:camera];
    
    return insert ? [self indexPathOfCameraWithId:camera.cameraId andServerId:camera.server.serverId] : nil;
}

-(HDWServerModel*) findServerById: (NSNumber*) serverId {
    return [servers objectForKey:serverId];
}

-(HDWCameraModel*) findCameraById: (NSNumber*) cameraId atServer: (NSNumber*) serverId {
    HDWServerModel *server = [self findServerById:serverId];
    if (server == nil)
        return nil;
    
    return [server findCameraById:cameraId];
}

-(HDWServerModel*) serverAtIndex: (NSInteger) index {
    return [servers.allValues objectAtIndex: index];
}

-(NSUInteger) indexOfServerWithId: (NSNumber*) serverId {
    return [servers.allKeys indexOfObject:serverId];
}

-(HDWCameraModel*) cameraAtIndexPath: (NSIndexPath*) indexPath {
    HDWServerModel *server = [self serverAtIndex:indexPath.section];
    return [server cameraAtIndex:indexPath.row];
}

-(NSIndexPath*) indexPathOfCameraWithId: (NSNumber*) cameraId andServerId: (NSNumber*) serverId {
    HDWServerModel *server = [self findServerById:serverId];
    
    NSInteger section = [self indexOfServerWithId:serverId];
    NSInteger row = [server indexOfCameraWithId:cameraId];
    
    return [NSIndexPath indexPathForRow:row inSection:section];
}

-(void) removeServerById: (NSNumber*) serverId {
    [servers removeObjectForKey:serverId];
}

-(void) removeCameraById: (NSNumber*) cameraId andServerId: (NSNumber*) serverId {
    HDWCameraModel *camera = [self findCameraById:cameraId atServer:serverId];
    [camera.server removeCameraById:cameraId];
}

-(int) count {
    return servers.count;
}

@end
