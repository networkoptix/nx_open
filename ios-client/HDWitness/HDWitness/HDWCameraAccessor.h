//
//  HDWCameraAccessor.h
//  HDWitness
//
//  Created by Ivan Vigasin on 01/02/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#ifndef HDWitness_HDWCameraAccessor_h
#define HDWitness_HDWCameraAccessor_h

#import "HDWCameraModel.h"

@protocol HDWCameraAccessor <NSObject>

- (HDWCameraModel *)cameraForUniqueId:(NSString *)uniqueId;

@end

#endif
