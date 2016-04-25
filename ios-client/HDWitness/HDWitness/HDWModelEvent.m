//
//  HDWModelEvent.m
//  HDWitness
//
//  Created by Ivan Vigasin on 5/1/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWModelEvent.h"

@implementation HDWModelEvent {
    BOOL _skipViewUpdate;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _skipViewUpdate = NO;
    }
    
    return self;
}

- (void)setSkipViewUpdate:(BOOL)skipViewUpdate {
    _skipViewUpdate = skipViewUpdate;
}

- (BOOL)isSkipViewUpdate {
    return _skipViewUpdate;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    return nil;
}

- (void)processView:(UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
    
}

- (NSString *)description {
    return @"HDWModelEvent: empty";
}

@end

@implementation HDWCameraAddEvent

- (HDWCameraAddEvent*)initWithCamera:(HDWCameraModel*)camera andIndexPath:(NSIndexPath *)indexPath {
    self = [super init];
    if (self) {
        _camera = camera;
        _indexPath = indexPath;
    }
    
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    [model insertCamera:_camera atIndexPath:_indexPath];
    return _indexPath;
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
#ifdef DEBUG_COLLECTION_VIEW
   NSLog(@"View: Inserting camera at %@", indexPath);
#endif
   [view insertItemsAtIndexPaths:@[indexPath]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWCameraAddEvent (%d-%d): ID=%@, NAME=%@, MAC=%@",
            [_indexPath indexAtPosition:0], [_indexPath indexAtPosition:1],
            [_camera.cameraId toString], _camera.name, _camera.physicalId];
}

@end

@implementation HDWCameraRemoveEvent {
    HDWResourceId *_cameraId;
}

- (HDWCameraRemoveEvent*)initWithCameraId:(HDWResourceId*)cameraId {
    self = [super init];
    if (self) {
        _cameraId = cameraId;
    }
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    NSIndexPath *indexPath = [model indexPathOfCameraWithId:_cameraId];
    if (indexPath) {
        [model removeCameraById:_cameraId];
    }

    return indexPath;
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"View: Removing camera at %@", indexPath);
#endif
    [view deleteItemsAtIndexPaths:@[indexPath]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWCameraRemoveEvent: ID=%@", [_cameraId toString]];
}

@end

@implementation HDWCameraChangeEvent

- (HDWCameraChangeEvent*)initWithCamera:(HDWCameraModel*)camera {
    self = [super init];
    if (self) {
        _camera = camera;
    }
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    [model addOrReplaceCamera:_camera];
    return [model indexPathOfCameraWithId:_camera.cameraId andServerId:_camera.server.serverId];
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"View: Changing camera at %@", indexPath);
#endif
    [view reloadItemsAtIndexPaths:@[indexPath]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWCameraChangeEvent: ID=%@", [_camera.cameraId toString]];
}

@end

@implementation HDWServerAddEvent

- (HDWServerAddEvent*)initWithServer:(HDWServerModel*)server andSection:(NSInteger)section {
    self = [super init];
    if (self) {
        _server = [server copy];
        _section = section;
    }
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    [model insertServer:_server atPosition:_section];
    return [NSIndexPath indexPathWithIndex:_section];
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
    NSUInteger position = [indexPath indexAtPosition:0];
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"View: Adding server at %d", position);
#endif
    [view insertSections:[NSIndexSet indexSetWithIndex:position]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWServerAddEvent: ID=%s NAME=%@", [_server.serverId toString], _server.name];
}

@end

@implementation HDWServerRemoveEvent {
    HDWResourceId *_serverId;
}

- (HDWServerRemoveEvent*)initWithServerId:(HDWResourceId*)serverId {
    self = [super init];
    if (self) {
        _serverId = serverId;
    }
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    NSUInteger serverIndex = [model indexOfServerWithId:_serverId];
    if (serverIndex == NSNotFound)
        return nil;
    
    [model removeServerById:_serverId];
    return [NSIndexPath indexPathWithIndex:serverIndex];
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath{
    NSUInteger position = [indexPath indexAtPosition:0];
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"View: Removing server at %d", position);
#endif
    [view deleteSections:[NSIndexSet indexSetWithIndex:position]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWServerRemoveEvent: ID=%@", [_serverId toString]];
}

@end

@implementation HDWServerChangeEvent {
    HDWServerModel *_server;
}

- (HDWServerChangeEvent*)initWithServer:(HDWServerModel*)server {
    self = [super init];
    if (self) {
        _server = server;
    }
        
    return self;
}

- (NSIndexPath*)processModel: (HDWECSModel*)model {
    NSUInteger serverIndex = [model indexOfServerWithId:_server.serverId];
    if (serverIndex == NSNotFound)
        return nil;
    
    HDWServerModel *server = [model serverAtIndex:serverIndex];
    [server setName:_server.name];
    [server setStatus:_server.status];
    return [NSIndexPath indexPathWithIndex:serverIndex];
}

- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath {
    NSUInteger position = [indexPath indexAtPosition:0];
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"View: Changing server at %d", position);
#endif
    [view reloadSections:[NSIndexSet indexSetWithIndex:position]];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"HDWServerChangeEvent: ID=%@, NAME=%@", [_server.serverId toString], _server.name];
}

@end
