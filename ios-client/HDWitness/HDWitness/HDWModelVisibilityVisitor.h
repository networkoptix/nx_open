//
//  HDWModelVisibilityVisitor.h
//  HDWitness
//
//  Created by Ivan Vigasin on 5/2/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWModelVisitor.h"

@interface HDWFilter: NSObject
- (BOOL)filterServer:(HDWServerModel*)server;
- (BOOL)filterCamera:(HDWCameraModel*)camera;
@end

@interface HDWDisabledCameraFilter: HDWFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera;
@end

@interface HDWOfflineCameraFilter: HDWFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera;
@end

@interface HDWAuthorizedCameraFilter: HDWFilter
- (BOOL)filterCamera:(HDWCameraModel*)camera;
@end

@interface HDWNoCamerasServerFilter: HDWFilter
- (BOOL)filterServer:(HDWServerModel*)server;
@end
    
@interface HDWModelVisibilityVisitor: NSObject<HDWModelVisitor>

- (HDWModelVisibilityVisitor*)initWithPre:(NSArray *)preFilters andPostFilters:(NSArray *)postFilters;
- (id)addUserFilter:(HDWFilter*)filter;
- (void)removeUserFilter:(id)filter;
- (HDWECSModel*)sort:(HDWECSModel*)model;

- (HDWECSModel*)processFiltersForModel:(HDWECSModel*)model;

@end

