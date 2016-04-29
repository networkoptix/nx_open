//
//  HDWModelVisibilityVisitor.m
//  HDWitness
//
//  Created by Ivan Vigasin on 5/2/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWModelVisibilityVisitor.h"
#import "HDWCameraModel.h"
#import "HDWECSConfig.h"

@implementation HDWFilter
- (BOOL)filterServer:(HDWServerModel*)server {
    return YES;
}

- (BOOL)filterCamera:(HDWCameraModel*)camera {
    return YES;
}
@end

@implementation HDWDisabledCameraFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera {
    return !camera.disabled;
}
@end

@implementation HDWOfflineCameraFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera {
    return camera.calculatedStatus.intValue != Status_Offline && camera.server.status.intValue != Status_Offline;
}
@end

@implementation HDWAuthorizedCameraFilter: HDWFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera {
    HDWECSModel *ecs = camera.server.ecs;
    return [ecs isCamera:camera accessibleToUser:ecs.config.login];
}
@end

@implementation HDWNoCamerasServerFilter
- (BOOL)filterServer:(HDWServerModel*)server {
    return server.cameras.count != 0;
}
@end

@implementation HDWModelVisibilityVisitor {
    NSArray *_preFilters;
    NSMutableArray *_userFilters;
    NSArray *_postFilters;
}

- (HDWModelVisibilityVisitor*)initWithPre:(NSArray *)preFilters andPostFilters:(NSArray *)postFilters {
    self = [super init];
    if (self) {
        _preFilters = preFilters;
        _postFilters = postFilters;
        _userFilters = [NSMutableArray array];
    }
    return self;
}

- (id)addUserFilter:(HDWFilter*)filter {
    [_userFilters addObject:filter];
    return filter;
}

- (void)removeUserFilter:(id)filter {
    [_userFilters removeObject:filter];
}

- (HDWECSModel *)processFilter:(HDWFilter *)filter forModel:(HDWECSModel *)model {
    HDWECSModel *newModel = [[HDWECSModel alloc] init];
    HDWServerModel *server;
    NSEnumerator *valuesEnumerator = [model.servers valuesEnumerator];
    while (server = [valuesEnumerator nextObject]) {
        if (![filter filterServer:server])
            continue;
        
        HDWServerModel *newServer = [server copy];
        [newModel addServer:newServer];
        
        HDWCameraModel *camera;
        NSEnumerator *camerasEnumerator = [server.cameras valuesEnumerator];
        while (camera = [camerasEnumerator nextObject]) {
            if ([filter filterCamera:camera]) {
                [newServer addOrReplaceCamera:[camera copy]];
            }
        }
    }
    
    return newModel;
}

- (HDWECSModel *)processFiltersForModel:(HDWECSModel*)model {
    NSArray *allFilters = [[_preFilters arrayByAddingObjectsFromArray:_userFilters] arrayByAddingObjectsFromArray:_postFilters];
    
    for (HDWFilter *filter in allFilters) {
        model = [self processFilter:filter forModel:model];
    }

    model = [self sort:model];
    return model;
}

- (HDWECSModel *)sort:(HDWECSModel*)model {
    NSArray *sortedServers = [[model.servers.valuesEnumerator allObjects] sortedArrayUsingSelector:@selector(compare:)];

    [model removeAllServers];
    for (HDWServerModel *server in sortedServers) {
        NSArray *sortedCameras = [[server.cameras.valuesEnumerator allObjects] sortedArrayUsingSelector:@selector(compare:)];
        
        [server removeAllCameras];
        for (HDWCameraModel *camera in sortedCameras)
            [server addOrReplaceCamera:camera];
        
        [model addServer:server];
    }
    
    return model;
}

@end
