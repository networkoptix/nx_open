//
//  HDWCameraModel.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//
#define QN_NO_NAMESPACES
#import "common/user_permissions.h"

#import "HDWMap.h"
#import "HDWUUID.h"
#import "HDWCameraModel.h"
#import "NSString+OptixAdditions.h"
#import "NSString+Base64.h"
#import "NSString+BasicAuthUtils.h"
#import "resource.pb.h"
#import "server.pb.h"
#import "camera.pb.h"
#import "layout.pb.h"
#import "user.pb.h"

#import "AFHTTPRequestOperation.h"

@interface HDWResourceId()

@property(nonatomic, copy) NSString* stringValue;

@end

@implementation HDWResourceId

+ (instancetype)resourceIdWithInt:(int)resourceId {
    return [[HDWResourceId alloc] initWithInt:resourceId];
}

+ (instancetype)resourceIdWithUuid:(HDWUUID *)resourceId {
    return [[HDWResourceId alloc] initWithUuid:resourceId];
}

+ (instancetype)resourceIdWithString:(NSString *)resourceId {
    return [[HDWResourceId alloc] initWithString:resourceId];
}

- (instancetype)initWithString:(NSString *)resourceId {
    if (self = [super init]) {
        self.stringValue = resourceId;
    }
    
    return self;
}

- (instancetype)initWithInt:(int)resourceId {
    return [self initWithString:[NSString stringWithFormat:@"%d", resourceId]];
}

- (instancetype)initWithUuid:(HDWUUID *)resourceId {
    return [self initWithString:resourceId.UUIDString];
}

- (BOOL)isEqual:(id)other
{
    if (other == self) {
        return YES;
    } else {
        return [self.stringValue isEqualToString:((HDWResourceId *)other).stringValue];
    }
}

- (NSUInteger)hash
{
    return self.stringValue.hash;
}

- (NSString *)toString {
    return self.stringValue;
}

- (HDWUUID *)toUUID {
    return [[HDWUUID alloc] initWithUUIDString:self.stringValue];
}

- (id) copyWithZone:(NSZone *)zone {
    HDWResourceId *result = [[HDWResourceId alloc] init];
    result.stringValue = self.stringValue;
    return result;
}

- (NSString *)description {
    return [self toString];
}

@end

@implementation HDWStreamDescriptor

- (id)initWithProtocol:(VideoTransport)transport transcoding:(BOOL)transcoding resolution:(NSString *)resolution andRequestParameter:(NSString *)reqParam  {
    self = [super init];
    if (self) {
        _transport = transport;
        _transcoding = transcoding;
        _resolution = [[HDWResolution alloc] initWithString:resolution];
        _quality = Q_UNDEFINED;
        _requestParameter = reqParam;
    }
    
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%s, %@", (self.transport == VT_HLS ? "hls" : "mjpeg"), self.resolution.description];
}

@end

@implementation HDWCameraServerItem

-(instancetype) initWithServer:(HDWServerModel *)server andTimestamp:(int64_t)timestamp {
    self = [super init];
    if (self) {
        _server = server;
        _timestamp = timestamp;
    }

    return self;
}

@end

@interface HDWCameraServerItems : NSObject

@property(nonatomic,readonly) HDWMap *items;


- (void)addItem:(HDWCameraServerItem *)item;

- (HDWServerModel *)serverForExactDate:(NSTimeInterval)elem withChunks:(NSDictionary *)serverChunks;
- (HDWServerModel *)onlinServerForFutureDate:(NSTimeInterval)elem withChunks:(NSDictionary *)serverChunks;

@end

@implementation HDWCameraServerItems

- (instancetype)init
{
    self = [super init];
    if (self) {
        _items = [[HDWMap alloc] init];
    }
    
    return self;
}

- (void)addItem:(HDWCameraServerItem *)item {
    [_items insertObject:item forKey:item.timestamp];
}

- (HDWServerModel *)serverForExactDate:(NSTimeInterval)date withChunks:(NSDictionary *)serverChunks {
    int64_t msecs = date * 1000;
    HDWCameraServerItem *item = [_items serverForExactDate:msecs];
    return item ? item.server : nil;
}

- (HDWServerModel *)onlinServerForFutureDate:(NSTimeInterval)date withChunks:(NSDictionary *)serverChunks {
    int64_t msecs = date * 1000;
    HDWCameraServerItem *item = [_items onlinServerForFutureDate:msecs withChunks:serverChunks];
    return item ? item.server : nil;
}

@end

@interface HDWChunks()

@property(nonatomic) NSArray* chunks;

@end

@implementation HDWChunks : NSObject

- (instancetype)initWithArray:(NSArray *)array;
{
    self = [super init];
    if (self) {
        _chunks = array;
    }
    
    return self;
}

- (int64_t)chunkStartForLaterDateMsecs:(int64_t)msecs {
    for (int i = 0; i < _chunks.count; i++) {
        NSArray *chunk = _chunks[i];
        int64_t startTime = [chunk[0] longLongValue];
        int64_t duration = [chunk[1] longLongValue];
        if (startTime > msecs)
            return startTime;
        
        if (duration == -1 || startTime + duration > msecs)
            return startTime;
    }
    
    return -1;
}

- (BOOL)hasChunkMatchingDateMsecs:(int64_t)msecs {
    for (int i = 0; i < _chunks.count; i++) {
        NSArray *chunk = _chunks[i];
        int64_t startTime = [chunk[0] longLongValue];
        int64_t duration = [chunk[1] longLongValue];
        if (startTime > msecs)
            return NO;
        
        if (duration == -1 || startTime + duration > msecs)
            return YES;
    }
    
    return NO;
}

@end

@implementation HDWCameraModel {
    HDWCameraServerItems *_cameraServerItems;
}

- (NSNumber *)calculatedStatus {
    if (self.server.status.intValue == Status_Offline)
        return Status_Offline;
    
    return _status;
}

- (NSArray *)orderedNativeResolutions:(NSDictionary *)mediaStreams {
    NSMutableArray *nativeResolutions = [NSMutableArray array];
    for (NSDictionary *mediaStream in mediaStreams[@"streams"]) {
        for (NSString *t in mediaStream[@"transports"]) {
            if ([t isEqualToString:@"hls"]) {
                NSString *res = mediaStream[@"resolution"];
                [nativeResolutions addObject:[[HDWResolution alloc] initWithString:res]];
            }
        }
    }
    
    [nativeResolutions sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
        HDWResolution *r1 = (HDWResolution *)obj1;
        HDWResolution *r2 = (HDWResolution *)obj2;
        
        if (r1.width > r2.width)
            return NSOrderedAscending;
        else if (r1.width < r2.width)
            return NSOrderedDescending;
        else
            return NSOrderedSame;
    }];
    
    return nativeResolutions;
}

- (HDWCameraModel*) initWithDict:(NSDictionary*) dict andServer:(HDWServerModel*) server {
    self = [super init];
    
    if (self) {
        _cameraId = dict[@"id"];
        _name = dict[@"name"];
        _status = dict[@"status"];
        _disabled = ((NSNumber*)dict[@"disabled"]).intValue == 1;
        _physicalId = dict[@"physicalId"];
        _secondaryStreamQuality = dict[@"secondaryStreamQuality"];

        NSArray *requestParams = @[@"hi", @"lo"];
        NSArray *nativeResolutions = [self orderedNativeResolutions:dict];
        
        NSMutableDictionary *resolutionRequestStrings = [NSMutableDictionary dictionary];
        for (int i = 0; i < MIN(nativeResolutions.count, 2); i++) {
            resolutionRequestStrings[[(HDWResolution *)(nativeResolutions[i]) originalString]] = requestParams[i];
        }
        
        NSMutableArray *resolutions = [NSMutableArray array];
        for (NSDictionary *mediaStream in dict[@"streams"]) {
            for (NSString *t in mediaStream[@"transports"]) {
                VideoTransport transport;
                if ([t isEqualToString:@"hls"])
                    transport = VT_HLS; // continue; //
                else if ([t isEqualToString:@"mjpeg"])
                    transport = VT_MJPEG;
                else
                    continue;
                
                NSString *resolutionString = mediaStream[@"resolution"];
                NSString *requestParameter = [resolutionRequestStrings objectForKey:resolutionString];
                if ([requestParameter isEqualToString:@"lo"] && [_secondaryStreamQuality isEqualToString:@"SSQualityDontUse"])
                    continue;
                
                HDWStreamDescriptor *resolution = [[HDWStreamDescriptor alloc] initWithProtocol:transport transcoding:[mediaStream[@"transcodingRequired"] boolValue] resolution:resolutionString andRequestParameter:requestParameter];
                HDWResolution *res = resolution.resolution;
                if (transport == VT_HLS && (res.width > 1920 || res.height > 1920 || (res.height > 1080 && res.width > 1080)))
                    continue;
                
                [resolutions addObject:resolution];
            }
        }
        
        _angle = dict[@"angle"];
        _aspectRatio = dict[@"aspectRatio"];
        
        _streamDescriptors = resolutions;
        _server = server;
        _cameraServerItems = [[HDWCameraServerItems alloc] init];
        _serverChunks = [NSMutableDictionary dictionary];
    }
    
    return self;
}

- (BOOL)onlineOrRecording {
    return _status.intValue == Status_Online || _status.intValue == Status_Recording; 
}

- (id)copyWithZone:(NSZone *)zone {
    HDWCameraModel *newCamera = [[HDWCameraModel allocWithZone:zone] init];
    
    if (newCamera) {
        newCamera->_cameraId = [_cameraId copyWithZone:zone];
        newCamera->_disabled = _disabled;
        newCamera->_name = [_name copyWithZone:zone];
        newCamera->_physicalId = [_physicalId copyWithZone:zone];
        newCamera->_server = _server;
        newCamera->_status = [_status copyWithZone:zone];
        newCamera->_streamDescriptors = [_streamDescriptors copyWithZone:zone];
        newCamera->_cameraServerItems = _cameraServerItems;
        newCamera->_serverChunks = _serverChunks;
        newCamera->_angle = _angle;
        newCamera->_aspectRatio = _aspectRatio;
    }
    
    return newCamera;
}

- (BOOL)hasChunks {
    return _serverChunks.count > 0;
}

- (BOOL)hasChunkForLaterDate:(NSTimeInterval)date {
    // First search current server
    if (self.server.status.intValue == Status_Online) {
        int64_t chunkStart = [_serverChunks[self.server.serverId] chunkStartForLaterDateMsecs:date*1000];
        if (chunkStart != -1) {
            return YES;
        }
    }

    return [_cameraServerItems onlinServerForFutureDate:date withChunks:_serverChunks] != nil;
}

- (BOOL)hasChunkForExactDate:(NSTimeInterval)date {
    HDWServerModel *server = [_cameraServerItems serverForExactDate:date withChunks:nil];
    if (!server || server.status.intValue == Status_Offline)
        server = self.server;
        
    if (server.status.intValue == Status_Offline)
        return NO;
    
    HDWChunks *chunks = _serverChunks[server.serverId];
    int64_t msecs = date * 1000;

    return [chunks hasChunkMatchingDateMsecs:msecs];
}

- (void)setChunks:(HDWChunks *)chunks forServer:(HDWServerModel *)server {
    _serverChunks[server.serverId] = chunks;
}

- (void)addCameraServerItem:(HDWCameraServerItem *)item {
    [_cameraServerItems addItem:item];
}

// Returns server which was this camera assigned to at a given date
- (HDWServerModel *)onlinServerForFutureDate:(NSTimeInterval)date withChunks:(NSDictionary *)serverChunks {
    HDWServerModel *server = [_cameraServerItems onlinServerForFutureDate:date withChunks:serverChunks];
    return server ? server : self.server;
}

- (BOOL)isEqual:(id)object {
    return [self isEqualToCamera:(HDWCameraModel *)object];
}

- (BOOL)isEqualToCamera:(HDWCameraModel *)camera {
    return [camera.cameraId isEqual:self.cameraId] &&
        camera.disabled == self.disabled &&
        [camera.name isEqualToString:self.name] &&
        [camera.physicalId isEqualToString:self.physicalId] &&
        camera.status.intValue == self.status.intValue;
}

- (NSComparisonResult)compare:(HDWCameraModel *)camera {
    NSComparisonResult nameComparison = [self.name.lowercaseString compare:camera.name.lowercaseString];
    if (nameComparison == NSOrderedSame)
        return [self.cameraId.toString compare:camera.cameraId.toString];
    else
        return nameComparison;
}

- (NSString *)description {
    return self.name;
}

@end

@implementation HDWServerModel {
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _cameras = [HDWOrderedDictionary dictionary];
    }
    
    return self;
}

- (HDWServerModel *)initWithDict:(NSDictionary *)dict andECS:(HDWECSModel *)ecs {
    self = [self init];
    
    if (self) {
        _serverId = dict[@"id"];
        _name = dict[@"name"];
        _status = dict[@"status"];
        _netAddrList = [dict[@"netAddrList"] componentsSeparatedByString:@";"];
        _streamingBaseUrl = [NSURL URLWithString:dict[@"apiUrl"]];
        _guid = dict[@"guid"];
        _ecs = ecs;
    }
    
    return self;
}

- (HDWCameraModel *)cameraAtIndex:(NSUInteger)index {
    return [_cameras valueAtIndex:index];
}

- (void)update:(HDWServerModel*) server {
    _name = server.name;
    _streamingBaseUrl = server.streamingBaseUrl;
    _status = server.status;
}

- (HDWCameraModel *)findCameraById:(HDWResourceId *)cameraId {
    return _cameras[cameraId];
}

- (HDWCameraModel *)findCameraByPhysicalId:(NSString *)physicalId {
    HDWCameraModel *camera;
    NSEnumerator *valuesEnumerator = [_cameras valuesEnumerator];
    while (camera = [valuesEnumerator nextObject]) {
        if ([camera.physicalId isEqualToString:physicalId]) {
            return camera;
        }
    }
    
    return nil;
}

- (void)removeAllCameras {
    [_cameras removeAllObjects];
}

- (void)removeCameraById:(NSNumber *)cameraId {
    [_cameras removeObjectForKey:cameraId];
}

- (void)insertCamera:(HDWCameraModel *)camera atPosition:(NSInteger)position {
    [_cameras setObject:camera forKey:camera.cameraId atIndex:position];
    self.ecs.camerasById[camera.cameraId] = camera;
}

- (void)addOrReplaceCamera:(HDWCameraModel *) camera {
    _cameras[camera.cameraId] = camera;
    self.ecs.camerasById[camera.cameraId] = camera;
}

- (NSUInteger) cameraCount {
    return [_cameras count];
}

- (NSUInteger)indexOfCameraWithId:(NSNumber *)cameraId {
    return [_cameras indexOfKey:cameraId];
}

- (id)copyWithZone:(NSZone *)zone {
    HDWServerModel *newServer = [[HDWServerModel allocWithZone:zone] init];
    
    if (newServer) {
        newServer->_ecs = _ecs;
        newServer->_name = [_name copyWithZone:zone];
        newServer->_guid = [_guid copyWithZone:zone];
        newServer->_serverId = [_serverId copyWithZone:zone];
        newServer->_status = [_status copyWithZone:zone];
        newServer->_streamingBaseUrl = [_streamingBaseUrl copyWithZone:zone];
    }
    
    return newServer;
}

- (BOOL)isEqual:(id)object {
    return [self isEqualToServer:(HDWServerModel*)object];
}

- (BOOL)isEqualToServer:(HDWServerModel*)server {
    return [server.guid isEqual:self.guid] &&
        [server.name isEqualToString:self.name] &&
        [server.serverId isEqual:self.serverId] &&
        [server.status isEqualToNumber:self.status];
}

- (NSComparisonResult)compare:(HDWServerModel*)server {
    NSComparisonResult nameComparison = [self.name.lowercaseString compare:server.name.lowercaseString];
    if (nameComparison == NSOrderedSame)
        return [self.guid.toString compare:server.guid.toString];
    else
        return nameComparison;
}

- (NSString *)description {
    return self.name;
}

@end


@implementation HDWLayoutModel

- (HDWLayoutModel *)initWithDict:(NSDictionary *)dict andUser:(HDWUserModel *) user findInCameras:(NSDictionary *)camerasById {
    self = [super init];
    
    if (self) {
        _layoutId = dict[@"id"]; // [NSNumber numberWithInt:resource.id];
        _name = dict[@"name"];
        _items = [[HDWOrderedDictionary alloc] init];
        _userid = dict[@"parentId"]; //[NSNumber numberWithInt:resource.parentId];
        
        for (NSDictionary *item in dict[@"items"]) {
            if (item[@"resourceId"] != 0) {
                HDWCameraModel *camera = camerasById[item[@"resourceId"]];
                if (camera)
                    _items[camera.physicalId] = item;
            }
        }
        _user = user;
    }
    
    return self;
}

- (BOOL)hasCameraWithPhysicalId:(NSString *)physicalId {
    return [self.items indexOfKey:physicalId] != NSNotFound;
}
@end


@implementation HDWUserModel

- (HDWUserModel *)initWithDict:(NSDictionary *)dict andECS:(HDWECSModel *)ecs {
    self = [super init];
    
    if (self) {
        _userId = dict[@"id"];
        _name = [dict[@"name"] lowercaseString];
        _layouts = [[HDWOrderedDictionary alloc] init];

        _rights = [dict[@"rights"] unsignedLongLongValue];

        _isAdmin = [dict[@"isAdmin"] boolValue] || _rights & GlobalProtectedPermission;
        _isArchiveAccessible = [dict[@"isAdmin"] boolValue] || (_rights & (GlobalProtectedPermission | GlobalViewArchivePermission)) != 0;
        _ecs = ecs;
    }
    
    return self;
}

- (void)addOrReplaceLayout:(HDWLayoutModel *)layout {
    _layouts[layout.layoutId] = layout;
}

- (BOOL)hasCameraWithPhysicalId:(NSString *)physicalId {
    HDWLayoutModel *layout;
    NSEnumerator *valuesEnumerator = [self.layouts valuesEnumerator];
    while (layout = [valuesEnumerator nextObject]) {
        if ([layout hasCameraWithPhysicalId:physicalId])
            return YES;
    }
    
    return NO;
}

@end


@implementation HDWECSModel

- (instancetype)initWithECSConfig:(HDWECSConfig *)config {
    self = [super init];
    if (self) {
        _config = config;
        _servers = [HDWOrderedDictionary dictionary];
        _users = [HDWOrderedDictionary dictionary];
        _camerasById = [NSMutableDictionary dictionary];

    }
    
    return self;
}

- (instancetype)init {
    return [self initWithECSConfig:nil];
}

- (void)clear {
    [self.servers removeAllObjects];
}

- (void)addServers:(NSArray *) serversArray {
    for (HDWServerModel* server in serversArray) {
        [self addServer: server];
    }
}

- (void)addUsers:(NSArray *)users {
    for (HDWUserModel *user in users) {
        [self addUser:user];
    }
}

- (void)addServer:(HDWServerModel *)server {
    if ((self.servers)[server.serverId])
        return;

    _servers[server.serverId] = server;
}

- (void)addUser:(HDWUserModel *)user {
    if (_users[user.userId])
        return;
    
    _users[user.name] = user;
}

- (BOOL)isCamera:(HDWCameraModel *)camera accessibleToUser:(NSString *)username {
    HDWUserModel *user = (self.users)[username];
    if (!user)
        return NO;
    
    if (user.isAdmin)
        return YES;
    
    return [user hasCameraWithPhysicalId:camera.physicalId];
}

- (BOOL)isCameraArchive:(HDWCameraModel *)camera accessibleToUser:(NSString *)username {
    HDWUserModel *user = (self.users)[username];
    if (!user)
        return NO;
    
    return user.isArchiveAccessible;
}

- (void)insertServer:(HDWServerModel *)server atPosition:(NSInteger)position {
    [_servers setObject:server forKey:server.serverId atIndex:position];
}

- (NSUInteger)addOrUpdateServer:(HDWServerModel *)newServer {
    HDWServerModel *server = _servers[newServer.serverId];
    if (server) {
        [server update:newServer];
        return NSNotFound;
    } else {
        [self addServer:newServer];
        return [self indexOfServerWithId:newServer.serverId];
    }
}

- (void)updateServer:(HDWServerModel *)server {
    HDWServerModel *existingServer = [self findServerById:server.serverId];
    [existingServer update:server];
}

- (void)insertCamera:(HDWCameraModel *)camera atIndexPath:(NSIndexPath *)indexPath {
    HDWServerModel *server = (self.servers)[camera.server.serverId];
    [server insertCamera:camera atPosition:indexPath.item];
}

- (void)addOrReplaceCamera:(HDWCameraModel *)camera {
    HDWCameraModel *existingCamera = [self findCameraById:camera.cameraId];

    if (![existingCamera.server.serverId isEqual:camera.server.serverId])
        [self removeCameraById:camera.cameraId];
    
    HDWServerModel *server = (self.servers)[camera.server.serverId];
    [server addOrReplaceCamera:camera];
}

- (void)mergeCameraFromDict:(NSDictionary *)cameraDict {
    HDWCameraModel *existingCamera = [self findCameraById:cameraDict[@"id"]];
    if (!existingCamera) {
        HDWServerModel *server = [self findServerById:cameraDict[@"parentId"]];
        HDWCameraModel *camera = [[HDWCameraModel alloc] initWithDict:cameraDict andServer:server];
        
        [self addOrReplaceCamera:camera];
    } else {
        for (NSString *key in cameraDict.allKeys) {
            if ([existingCamera respondsToSelector:NSSelectorFromString(key)]) {
                [existingCamera setValue:cameraDict[key] forKey:key];
            }
        }
    }
}

- (HDWServerModel *)findServerById:(NSNumber *)serverId {
    return _servers[serverId];
}

- (HDWServerModel *)findServerByGuid:(HDWResourceId *)guid {
    HDWServerModel *server;
    NSEnumerator *enumerator = [self.servers valuesEnumerator];
    while (server = [enumerator nextObject]) {
        if ([server.guid isEqual:guid])
            return server;
    }
    
    return nil;
}

- (HDWCameraModel *)findCameraById:(HDWResourceId *)cameraId {
    HDWServerModel *server;
    NSEnumerator *valuesEnumerator = [self.servers valuesEnumerator];
    while (server = [valuesEnumerator nextObject]) {
        HDWCameraModel *camera = [server findCameraById:cameraId];
        if (camera && !camera.disabled)
            return camera;
    }
    
    return nil;
}

- (HDWCameraModel *)findCameraByPhysicalId:(NSString *)physicalId {
    HDWServerModel *server;
    NSEnumerator *valuesEnumerator = [self.servers valuesEnumerator];
    while (server = [valuesEnumerator nextObject]) {
        HDWCameraModel *camera = [server findCameraByPhysicalId:physicalId];
        if (camera && !camera.disabled)
            return camera;
    }
    
    return nil;
}

- (HDWCameraModel *)findCameraById:(HDWResourceId *)cameraId atServer:(HDWResourceId *)serverId {
    HDWServerModel *server = [self findServerById:serverId];
    if (server == nil)
        return nil;
    
    return [server findCameraById:cameraId];
}

- (HDWUserModel *)findUserById:(HDWResourceId *)userId {
    HDWUserModel *user;
    
    HDWUserModel *u;
    NSEnumerator *valuesEnumerator = [self.users valuesEnumerator];
    while (u = [valuesEnumerator nextObject]) {
        if ([u.userId isEqual:userId]) {
            user = self.users[u.name];
            break;
        }
    }
    
    return user;
}

- (HDWServerModel *)serverAtIndex:(NSInteger)index {
    return [_servers valueAtIndex:index];
}

- (NSUInteger)indexOfServerWithId:(HDWResourceId *)serverId {
    return [_servers indexOfKey:serverId];
}

- (HDWCameraModel *)cameraAtIndexPath:(NSIndexPath *)indexPath {
    HDWServerModel *server = [self serverAtIndex:indexPath.section];
    return [server cameraAtIndex:indexPath.row];
}

- (NSIndexPath *)indexPathOfCameraWithId:(HDWResourceId *)cameraId {
    HDWCameraModel *camera = [self findCameraById:cameraId];
    HDWServerModel *server = (self.servers)[camera.server.serverId];
    
    NSInteger section = [self indexOfServerWithId:camera.server.serverId];
    NSInteger row = [server indexOfCameraWithId:cameraId];
    
    return [NSIndexPath indexPathForItem:row inSection:section];
}

- (NSIndexPath *)indexPathOfCameraWithId:(HDWResourceId *)cameraId andServerId:(HDWResourceId *)serverId {
    HDWServerModel *server = [self findServerById:serverId];
    
    NSInteger section = [self indexOfServerWithId:serverId];
    NSInteger row = [server indexOfCameraWithId:cameraId];
    
    return [NSIndexPath indexPathForItem:row inSection:section];
}

- (void)removeAllServers {
    [_servers removeAllObjects];
}

- (void) removeServerById:(NSNumber *)serverId {
    [_servers removeObjectForKey:serverId];
}

- (void)removeCameraById:(HDWResourceId *)cameraId  {
    HDWCameraModel *camera = [self findCameraById:cameraId];
    if (camera) {
        HDWServerModel *server = [self findServerById:camera.server.serverId];
        [server removeCameraById:cameraId];
    }
    [_camerasById removeObjectForKey:cameraId];
}

- (void)removeCameraById:(HDWResourceId *)cameraId andServerId:(HDWResourceId *)serverId {
    HDWServerModel *server = (self.servers)[serverId];
    [server removeCameraById:cameraId];
}

- (NSUInteger)count {
    return _servers.count;
}

- (id)copyWithZone:(NSZone *)zone {
    HDWECSModel *newModel = [[HDWECSModel alloc] init];
    HDWServerModel *server;
    
    NSEnumerator *valuesEnumerator = [self.servers valuesEnumerator];
    while (server = [valuesEnumerator nextObject]) {
        HDWServerModel *newServer = [server copy];
        [newModel addServer:newServer];
        
        HDWCameraModel *camera;
        NSEnumerator *camerasEnumerator = [server.cameras valuesEnumerator];
        while (camera = [camerasEnumerator nextObject]) {
            [newServer addOrReplaceCamera:[camera copy]];
        }
    }
    
    return newModel;
}

@end