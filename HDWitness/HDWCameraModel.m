//
//  HDWCameraModel.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWCameraModel.h"

@implementation HDWECSConfig
+ (HDWECSConfig*) defaultConfig {
    HDWECSConfig* instance = [[HDWECSConfig alloc] init];
    instance.name = @"Server";
    instance.host = @"";
    instance.port = @"7001";
    instance.login = @"admin";
    instance.password = @"";
    
    return instance;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    //Encode properties, other class variables, etc
    [encoder encodeObject:self.name forKey:@"name"];
    [encoder encodeObject:self.host forKey:@"host"];
    [encoder encodeObject:self.port forKey:@"port"];
    [encoder encodeObject:self.login forKey:@"login"];
    [encoder encodeObject:self.password forKey:@"password"];
}

- (id)initWithCoder:(NSCoder *)decoder {
    if((self = [super init])) {
        //decode properties, other class vars
        self.name = [decoder decodeObjectForKey:@"name"];
        self.host = [decoder decodeObjectForKey:@"host"];
        self.port = [decoder decodeObjectForKey:@"port"];
        self.login = [decoder decodeObjectForKey:@"login"];
        self.password = [decoder decodeObjectForKey:@"password"];
    }
    return self;
}

-(NSURL*) url {
    return [NSURL URLWithString:[NSString stringWithFormat:@"https://%@:%@@%@:%@", _login, _password, _host, _port]];
}
@end

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

-(void) setStatus: (NSNumber*) newStatus {
    _status = newStatus;
}

-(void) setDisabled: (BOOL) newDisabled {
    _disabled = newDisabled;
}

-(NSURL*) videoUrl {
    NSString *path = [NSString stringWithFormat:@"/proxy/http/%@:%@/media/%@.mpjpeg?resolution=240p", _server.streamingUrl.host, _server.streamingUrl.port, _physicalId];
    return [NSURL URLWithString:path relativeToURL:_server.ecs.config.url];
}

-(NSURL*) thumbnailUrl {
    NSString *path = [NSString stringWithFormat:@"/proxy/http/%@:%@/api/image?physicalId=%@&time=now&width=160", _server.streamingUrl.host, _server.streamingUrl.port, _physicalId];
    return [NSURL URLWithString:path relativeToURL:_server.ecs.config.url];
}

@end

@implementation HDWServerModel

-(HDWServerModel*) initWithDict: (NSDictionary*) dict andECS: (HDWECSModel*) ecs {
    self = [super init];
    
    if (self) {
        _serverId = dict[@"id"];
        _name = dict[@"name"];
        _status = dict[@"status"];
        _streamingUrl = [NSURL URLWithString:dict[@"streamingUrl"]];
        _ecs = ecs;
        
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
    _status = server.status;
}

-(void) setStatus: (NSNumber*) newStatus {
    _status = newStatus;
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

@implementation HDWECSModel

-(id)initWithECSConfig: (HDWECSConfig*)config {
    self = [super init];
    if (self) {
        ecsConfig = config;
        servers = [[NSMutableDictionary alloc] init];
    }
    
    return self;
}

-(HDWECSConfig*) config {
    return ecsConfig;
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
        [self addServer:newServer];
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
