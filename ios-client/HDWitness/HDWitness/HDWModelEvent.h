//
//  HDWModelEvent.h
//  HDWitness
//
//  Created by Ivan Vigasin on 5/1/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HDWCameraModel.h"

@interface HDWModelEvent : NSObject
- (instancetype)init;

- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;

- (void)setSkipViewUpdate:(BOOL)skipViewUpdate;
- (BOOL)isSkipViewUpdate;
@end

@interface HDWCameraAddEvent : HDWModelEvent
- (HDWCameraAddEvent*)initWithCamera:(HDWCameraModel*)camera andIndexPath:(NSIndexPath *)indexPath;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;

@property(nonatomic, readonly) HDWCameraModel *camera;
@property(nonatomic, readonly) NSIndexPath *indexPath;
@end

@interface HDWCameraRemoveEvent : HDWModelEvent
- (HDWCameraRemoveEvent*)initWithCameraId:(HDWResourceId *)cameraId;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;
@end

@interface HDWCameraChangeEvent : HDWModelEvent
- (HDWCameraChangeEvent*)initWithCamera:(HDWCameraModel*)camera;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;

@property(nonatomic, readonly) HDWCameraModel *camera;
@end

@interface HDWServerAddEvent : HDWModelEvent
- (HDWServerAddEvent*)initWithServer:(HDWServerModel*)server andSection:(NSInteger)section;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;

@property(nonatomic, readonly) HDWServerModel *server;
@property(nonatomic, readonly) NSInteger section;
@end

@interface HDWServerRemoveEvent : HDWModelEvent
- (HDWServerRemoveEvent*)initWithServerId:(HDWResourceId*)serverId;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;
- (NSString *)description;
@end

@interface HDWServerChangeEvent : HDWModelEvent
- (HDWServerChangeEvent*)initWithServer:(HDWServerModel*)server;
- (NSIndexPath*)processModel: (HDWECSModel*)model;
- (void)processView: (UICollectionView*)view atIndexPath:(NSIndexPath*)indexPath;
- (NSString *)description;
@end
