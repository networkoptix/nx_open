//
//  HDWPB2ProtocolImpl.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/26/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWPB2ProtocolImpl.h"
#import "HDWEventSource.h"

#import "resource.pb.h"
#import "server.pb.h"
#import "camera.pb.h"
#import "layout.pb.h"
#import "user.pb.h"
#import "connectinfo.pb.h"
#import "message.pb.h"

#import "HDWURLBuilder.h"
#import "HDWCameraModel.h"
#import "HDWECSConfig.h"
#import "HDWApiConnectInfo.h"
#import "HDWCompatibility.h"

@interface HDWPB2ProtocolImpl() {
    PBMutableExtensionRegistry *_extensionRegistry;
    NSArray *v22_streams;
}

- (instancetype)init;
- (NSURL*)videoUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date streamDescriptor:(HDWStreamDescriptor *)streamDescriptor andQuality:(NSUInteger)quality;

@end

@implementation HDWPB2ProtocolImpl

- (instancetype)init {
    self = [super init];
    if (self) {
        _extensionRegistry = [PBMutableExtensionRegistry registry];
        [ServerRoot registerAllExtensions:_extensionRegistry];
        [CameraRoot registerAllExtensions:_extensionRegistry];
        [LayoutRoot registerAllExtensions:_extensionRegistry];
        [UserRoot registerAllExtensions:_extensionRegistry];
        [MessageRoot registerAllExtensions:_extensionRegistry];

        v22_streams = @[ @{ @"transports": @[@"mjpeg", @"webm"], @"transcodingRequired": @YES, @"resolution": @"*"}];
    }
    
    return self;
}

- (BOOL)useBasicAuth {
    return YES;
}

- (NSString *)systemTimePath {
    return nil;
}

- (NSURL *)urlForConfig:(HDWECSConfig *)config {
    return [config urlForScheme:@"https"];
}

- (NSString *)eventsPath {
    return [NSString stringWithFormat:@"/events/?format=pb&guid=%@&isClient", [[HDWUUID UUID] UUIDString]];
}

- (Class)eventSourceClass {
    return [HDWEventSource class];
}

+ (HDWApiConnectInfo *)parseConnectInfo:(NSData *)data {
    HDWApiConnectInfo *result = [[HDWApiConnectInfo alloc] init];
    
    ConnectInfo *connectInfo;
    
    @try {
        connectInfo= [ConnectInfo parseFromData:data];
    } @catch (NSException *e) {
        return nil;
    }
    
    if (!connectInfo)
        return nil;
    
    if ([connectInfo hasBrand])
        result.brand = connectInfo.brand;
    
    result.version = connectInfo.version;
    
    NSMutableArray* items = [NSMutableArray array];
    for (CompatibilityItem *ci in connectInfo.compatibilityItems.item) {
        HDWCompatibilityItem *item = [HDWCompatibilityItem itemForVersion:ci.ver1 ofComponent:ci.comp1 andSystemVersion:ci.ver2];
        [items addObject:item];
    }
    
    result.compatibilityItems = items;

    return result;
}

- (NSDictionary *)pbServerToDict:(Resource *)resource {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithInt:resource.id];
    dict[@"name"] = resource.name;
    dict[@"guid"] = [HDWResourceId resourceIdWithString:resource.guid];
    dict[@"status"] = [NSNumber numberWithInt:resource.status];
    
    Server *serverPb = [resource getExtension:Server.resource];
    
    dict[@"netAddrList"] = serverPb.netAddrList;
    dict[@"apiUrl"] = serverPb.apiUrl;
    
    return dict;
}

- (NSDictionary *)pbCameraToDict:(Resource *)resource {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithInt:resource.id];
    dict[@"name"] = resource.name;
    dict[@"status"] = [NSNumber numberWithInt:resource.status];
    dict[@"disabled"] = [NSNumber numberWithInt:resource.disabled];
    
    Camera *camera = [resource getExtension:Camera.resource];
    dict[@"physicalId"] = camera.physicalId;
    
    dict[@"streams"] = v22_streams;

    return dict;
}

- (NSDictionary *)pbUserToDict:(Resource *)resource {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];

    dict[@"id"] = [HDWResourceId resourceIdWithInt:resource.id];
    dict[@"name"] = resource.name;
    
    User *user = [resource getExtension:User.resource];
    dict[@"isAdmin"] = @(user.isAdmin);
    dict[@"rights"] = @(user.rights);
    
    return dict;
}

- (NSDictionary *)pbLayoutToDict:(Resource *)resource {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithInt:resource.id];
    dict[@"name"] = resource.name;
    dict[@"parentId"] = [HDWResourceId resourceIdWithInt:resource.parentId];
    
    dict[@"items"] = [NSMutableArray array];
    
    Layout *layout = [resource getExtension:Layout.resource];
    for (Layout_Item *item in layout.item) {
        [dict[@"items"] addObject:@{@"resourceId": [HDWResourceId resourceIdWithInt:item.resource.id]}];
    }

    return dict;
}

- (void)loadResources:(NSArray *)resources toModel:(HDWECSModel*)model {
    NSMutableDictionary* usersDict = [[NSMutableDictionary alloc] init];
    NSMutableDictionary* serversDict = [[NSMutableDictionary alloc] init];
    
    for (Resource *resource in resources) {
        if (resource.type == Resource_TypeServer) {
            HDWServerModel *server = [[HDWServerModel alloc] initWithDict:[self pbServerToDict:resource] andECS:model];
            serversDict[server.serverId] = server;
            
        } else if (resource.type == Resource_TypeUser) {
            HDWUserModel *user = [[HDWUserModel alloc] initWithDict:[self pbUserToDict:resource] andECS:model];
            usersDict[user.userId] = user;
        }
    }
    
    for (Resource *resource in resources) {
        if (resource.type == Resource_TypeCamera) {
            HDWServerModel *server = serversDict[[HDWResourceId resourceIdWithInt:resource.parentId]];
            HDWCameraModel *camera = [[HDWCameraModel alloc] initWithDict:[self pbCameraToDict:resource] andServer:server];
            (model.camerasById)[camera.cameraId] = camera;
            [server addOrReplaceCamera:camera];
        }
    }
    
    for (Resource *resource in resources) {
        if (resource.type == Resource_TypeLayout) {
            HDWUserModel *user = usersDict[[HDWResourceId resourceIdWithInt:resource.parentId]];
            HDWLayoutModel *layout = [[HDWLayoutModel alloc] initWithDict:[self pbLayoutToDict:resource] andUser:user findInCameras:model.camerasById];
            
            [user addOrReplaceLayout:layout];
        }
    }
    
    [model clear];
    [model addServers: serversDict.allValues];
    [model addUsers: usersDict.allValues];
}

- (void)loadCameraServerItems:(NSArray *)resources toModel:(HDWECSModel*)ecs {
    for (CameraServerItem *cameraServerItem in resources) {
        HDWCameraModel *camera = [ecs findCameraByPhysicalId:cameraServerItem.physicalId];
        if (!camera)
            continue;
            
        HDWServerModel *server = [ecs findServerByGuid:[HDWResourceId resourceIdWithString:cameraServerItem.serverGuid]];
        if (!server)
            continue;
        
        [camera addCameraServerItem:[[HDWCameraServerItem alloc] initWithServer:server andTimestamp:cameraServerItem.timestamp]];
    }
}

- (BOOL)processMessage:(NSData *)messageData withECS:(HDWECSModel *)ecs {
    Message *message = [Message parseFromData:messageData extensionRegistry:_extensionRegistry];
    
    switch (message.type) {
        case Message_TypeInitial: {
            InitialMessage *initialPb = [message getExtension:InitialMessage.message];
            [self loadResources:initialPb.resource toModel:ecs];
            [self loadCameraServerItems:initialPb.cameraServerItem toModel:ecs];
            break;
        }
        
        case Message_TypeResourceChange: {
            ResourceMessage *resourceMessage = [message getExtension:ResourceMessage.message];
            Resource *resource = resourceMessage.resource;
            
            if (resource.type == Resource_TypeServer) {
                HDWServerModel* server = [[HDWServerModel alloc] initWithDict:[self pbServerToDict:resource] andECS:ecs];
                [ecs addOrUpdateServer:server];
            } else if (resource.type == Resource_TypeCamera) {
                HDWServerModel *server = [ecs findServerById:[HDWResourceId resourceIdWithInt:resource.id]];
                HDWCameraModel *camera = [[HDWCameraModel alloc] initWithDict:[self pbCameraToDict:resource] andServer:server];
                
                [ecs addOrReplaceCamera:camera];
            }
            
            break;
        }
            
        case Message_TypeResourceDelete: {
            ResourceMessage *resourceMessage = [message getExtension:ResourceMessage.message];
            Resource *resource = resourceMessage.resource;
            
            HDWResourceId *resourceId = [HDWResourceId resourceIdWithInt:resource.id];
            HDWResourceId *parentId = [HDWResourceId resourceIdWithInt:resource.parentId];
            
            if ([ecs findServerById:resourceId])
                [ecs removeServerById:resourceId];
            else if ([ecs findCameraById:resourceId atServer:parentId]) {
                [ecs removeCameraById:resourceId andServerId:parentId];
            }
            
            break;
        }
            
        case Message_TypeResourceStatusChange: {
            ResourceMessage *resourceMessage = [message getExtension:ResourceMessage.message];
            Resource *resource = resourceMessage.resource;
            
            NSNumber *status = [NSNumber numberWithInt:resource.status];
            HDWResourceId *resourceId = [HDWResourceId resourceIdWithInt:resource.id];
            HDWResourceId *parentId = [HDWResourceId resourceIdWithInt:resource.parentId];
            
            if (parentId) {
                HDWCameraModel *camera = [ecs findCameraById:resourceId atServer:parentId];
                [camera setStatus:status];
            } else {
                HDWServerModel *server = [ecs findServerById:resourceId];
                [server setStatus:status];
            }
            
            break;
        }
            
        case Message_TypeResourceDisabledChange: {
            ResourceMessage *resourceMessage = [message getExtension:ResourceMessage.message];
            Resource *resource = resourceMessage.resource;
            
            NSNumber *disabled = [NSNumber numberWithInt:resource.disabled];
            HDWResourceId *resourceId = [HDWResourceId resourceIdWithInt:resource.id];
            HDWResourceId *parentId = [HDWResourceId resourceIdWithInt:resource.parentId];
            
            HDWCameraModel *camera = [ecs findCameraById:resourceId atServer:parentId];
            camera.disabled = disabled;
        }
            
        default:
            return NO;
    }
    
    
    return YES;
}

- (NSURL *)chunksUrlForCamera:(HDWCameraModel *)camera atServer:(HDWServerModel *)server {
    NSString *path = [NSString stringWithFormat:@"/proxy/http/%@:%@/api/RecordedTimePeriods?serverGuid=%@&physicalId=%@&startTime=0&endTime=4000000000000&periodsType=0&detail=60000&format=bin",
                      server.streamingBaseUrl.host, server.streamingBaseUrl.port,
                      [server.guid toString], camera.physicalId];
    NSURL *baseUrl = [self urlForConfig:server.ecs.config];
    NSURL *url = [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
#ifdef DEBUG_URLS
    NSLog(@"Chunks at Server %@: %@", server.serverId, url.absoluteString);
#endif
    return url;
}

- (NSURL*)videoUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date streamDescriptor:(HDWStreamDescriptor *)streamDescriptor andQuality:(NSUInteger)quality {
    NSString *position;
    
    NSString *path;
    if (date) {
        int64_t msecs = date * 1000ll;
        position = [NSString stringWithFormat:@"%lld", msecs];
    } else {
        position = @"now";
    }
    
    HDWServerModel *server = [camera onlinServerForFutureDate:date withChunks:camera.serverChunks];
    
    path = [NSString stringWithFormat:@"/proxy/http/%@:%@/media/%@.mpjpeg?serverGuid=%@&resolution=%dp&qmin=%d&qmax=%d&pos=%@&sfd=", server.streamingBaseUrl.host, server.streamingBaseUrl.port, camera.physicalId, [server.guid toString], streamDescriptor.resolution.height, quality, quality, position];
    
    NSURL *baseUrl = [self urlForConfig:server.ecs.config];
    NSURL *url = [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
#ifdef DEBUG_URLS
    NSLog(@"VideoUrl: %@", url.absoluteString);
#endif
    return url;
}

- (NSURL *)liveUrlForCamera:(HDWCameraModel *)camera andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor {
    return [self videoUrlForCamera:camera date:0 streamDescriptor:streamDescriptor andQuality:10];
}

- (NSURL *)archiveUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor {
    return [self videoUrlForCamera:camera date:date streamDescriptor:streamDescriptor andQuality:10];
}

- (NSURL *)thumbnailUrlForCamera:(HDWCameraModel *)camera; {
    NSString *path = [NSString stringWithFormat:@"/proxy/http/%@:%@/api/image?serverGuid=%@&physicalId=%@&time=latest&height=360&format=png",
                      camera.server.streamingBaseUrl.host, camera.server.streamingBaseUrl.port, [camera.server.guid toString], camera.physicalId];
    NSURL *baseUrl = [self urlForConfig:camera.server.ecs.config];
    return [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
}

@end
