//
//  HDWMultipartStreamParser.m
//  MultiPartParser
//
//  Created by Ivan Vigasin on 4/28/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWMultipartStreamParser.h"

#import "HDWQueue.h"

#include "multipart_parser.h"

@interface HDWMultipartStreamParser()
- (void)atPartDataBegin;
- (void)atPartDataEnd;
- (void)atPartData:(NSData *)data;
@end

static int at_partDataStatic(multipart_parser* p, const char *at, size_t length)
{
    HDWMultipartStreamParser* me = (__bridge HDWMultipartStreamParser *)multipart_parser_get_data(p);
    
    NSData *data = [NSData dataWithBytes:at length:length];
    [me atPartData:data];
    
    return 0;
}

static int at_partDataBeginStatic(multipart_parser* p)
{
    HDWMultipartStreamParser* me = (__bridge HDWMultipartStreamParser *)multipart_parser_get_data(p);

    [me atPartDataBegin];
    
    return 0;
}

static int at_partDataEndStatic(multipart_parser* p)
{
    HDWMultipartStreamParser* me = (__bridge HDWMultipartStreamParser *)multipart_parser_get_data(p);
    
    [me atPartDataEnd];
    
    return 0;
}

@implementation HDWMultipartStreamParser {
    NSString *_boundary;
    HDWQueue *_blocks;
    NSMutableData *_incomplete;
    
    multipart_parser* m_parser;
    multipart_parser_settings m_callbacks;
}

- (instancetype)initWithBoundary:(NSString *)boundary {
    self = [super init];
    if (self) {
        _boundary = boundary;
        _blocks = [[HDWQueue alloc] init];
        _incomplete = [NSMutableData data];
        
        memset(&m_callbacks, 0, sizeof(multipart_parser_settings));
        m_callbacks.on_part_data = at_partDataStatic;
        m_callbacks.on_part_data_begin = at_partDataBeginStatic;
        m_callbacks.on_part_data_end = at_partDataEndStatic;
        
        m_parser = multipart_parser_init(boundary.UTF8String, &m_callbacks);
        multipart_parser_set_data(m_parser, (void *)CFBridgingRetain(self));
    }
    
    return self;
}

- (void)dealloc
{
    CFBridgingRelease((__bridge CFTypeRef)(self));
    multipart_parser_free(m_parser);
}

- (void)reset {
    [_incomplete setLength:0];
}

- (void)addData:(NSData *)data {
    multipart_parser_execute(m_parser, data.bytes , data.length);
}

- (void)atPartDataBegin {
    [self reset];
}

- (void)atPartDataEnd {
    [_blocks addObject:_incomplete];
    _incomplete = [NSMutableData data];
}

- (void)atPartData:(NSData *)data {
    [_incomplete appendData:data];
}

- (NSData *)nextMessage {
    if ([_blocks isEmpty])
        return nil;
    
    return [_blocks takeObject];
}

@end
