//
//  HDWJSONProtocol.h
//  HDWitness
//
//  Created by Ivan Vigasin on 7/21/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWProtocol.h"

@interface HDWJSONProtocolImpl : NSObject<HDWProtocol>

- (id)init:(HDWApiConnectInfo *)connectInfo;
- (NSString *)systemTimePath;
- (BOOL)useBasicAuth;

@end
