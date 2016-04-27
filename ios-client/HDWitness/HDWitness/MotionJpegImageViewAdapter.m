//
//  MotionJpegImageViewAdapter.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/11/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "MotionJpegImageViewAdapter.h"

@implementation MotionJpegImageViewAdapter {
    __weak id<HDWOptixVideoViewDelegate> _delegate;
}

- (void)doInit {
    [super doInit];
 
    super.delegate = self;
}

- (void)setDelegate:(id<HDWOptixVideoViewDelegate>)delegate {
    _delegate = delegate;
}

- (void)setUrl:(NSURL *)url {
    [super setUrl:url];
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

- (BOOL)isPaused {
    return !super.isPlaying;
}

#pragma mark -
#pragma mark MotionJpegImageViewDelegate methods

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
