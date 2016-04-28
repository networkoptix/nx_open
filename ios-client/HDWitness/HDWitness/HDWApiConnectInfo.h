//
//  HDWApiConnectInfo.h
//  HDWitness
//
//  Created by Ivan Vigasin on 11/21/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWProtocol.h"

@class HDWUUID;

/**
 * Model representing ConnectInfo, received from EC
 */
@interface HDWApiConnectInfo : NSObject

@property(nonatomic,copy) NSString *brand;
@property(nonatomic,copy) NSString *version;
@property(nonatomic,copy) HDWUUID *serverGuid;
@property(nonatomic,copy) NSArray *compatibilityItems;

@end

