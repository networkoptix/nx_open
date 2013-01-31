//
//  HDWCameraModel.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/28/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface HDWServerModel : NSObject {
}

@property NSString* name;
@property NSString* url;
@property NSMutableArray* cameras;

@end

@interface HDWCameraModel : NSObject {
}

@property NSString* name;
@property NSString* url;

@end
