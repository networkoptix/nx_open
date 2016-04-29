//
//  HDWPB2ProtocolImpl.h
//  HDWitness
//
//  Created by Ivan Vigasin on 3/26/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWProtocol.h"

@interface HDWPB2ProtocolImpl : NSObject<HDWProtocol>

- (NSString *)systemTimePath;
- (BOOL)useBasicAuth;

@end
