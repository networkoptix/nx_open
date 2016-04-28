//
//  HDWLiveStreamingViewAdapter.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/17/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWLiveStreamingViewAdapter.h"

@implementation HDWLiveStreamingViewAdapter {
    __weak id<HDWOptixVideoViewDelegate> _delegate;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    super.delegate = self;
    return self;
}

- (void)setDelegate:(id<HDWOptixVideoViewDelegate>)delegate {
    _delegate = delegate;
}

- (void)play {
    [super play];
}

- (void)pause {
    [super pause];
}

- (void)stop {
    [super stop];
}

#pragma mark -
#pragma mark OVTPlayerLayerViewDelegate

- (void)onFirstFrameReceived {
    [_delegate onFirstFrameReceived];
}

- (void)onFrameReceived:(NSTimeInterval)timestamp andFps:(NSInteger)currentFps {
    [_delegate onFrameReceived:timestamp andFps:currentFps];
}

- (void)onConnectionClosed {
    [_delegate onConnectionClosed];
}

@end
