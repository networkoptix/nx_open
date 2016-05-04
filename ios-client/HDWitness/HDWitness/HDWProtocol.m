//
//  HDWProtocolFactory.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/26/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWProtocol.h"
#import "HDWCameraModel.h"

#import "HDWPB2ProtocolImpl.h"
#import "HDWJSONProtocolImpl.h"

@implementation HDWProtocolFactory

+ (HDWApiConnectInfo *)parseConnectInfo:(NSData *)data andDetectProtocol:(id<HDWProtocol> *)protocol {
    HDWApiConnectInfo *connectInfo;
    
    connectInfo = [HDWPB2ProtocolImpl parseConnectInfo:data];
    if (connectInfo) {
        *protocol = [[HDWPB2ProtocolImpl alloc] init];
        return connectInfo;
    }
    
    connectInfo = [HDWJSONProtocolImpl parseConnectInfo:data];
    if (connectInfo) {
        *protocol = [[HDWJSONProtocolImpl alloc] init:connectInfo];
        return connectInfo;
    }
    
    return nil;
        
}

@end
