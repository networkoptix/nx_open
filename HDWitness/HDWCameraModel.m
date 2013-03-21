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

-(HDWServerModel*) findServerById: (NSNumber*) id {
    for (HDWServerModel* server in servers) {
        if (server.serverId == id)
            return server;
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

-(int) count {
    return servers.count;
}

@end
