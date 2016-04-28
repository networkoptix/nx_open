//
//  HDWOptixVideoView.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/11/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWOptixVideoView.h"

#import "MotionJpegImageViewAdapter.h"
#import "HDWLiveStreamingViewAdapter.h"

@interface HDWOptixVideoView()

@property(nonatomic) UIView<HDWOptixVideoControl> *subview;

@end

@implementation HDWOptixVideoView {
    __weak id<HDWOptixVideoViewDelegate> _delegate;
}

- (void)setDelegate:(id<HDWOptixVideoViewDelegate>)delegate {
    _delegate = delegate;
}

- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSNumber *)angle andAspectRatio:(NSNumber *)aspectRatio {
    [_subview stop];
    
    if ([url.path hasSuffix:@".mpjpeg"]) {
        _subview = [[MotionJpegImageViewAdapter alloc] initWithFrame:self.frame];
    } else {
        _subview = [[HDWLiveStreamingViewAdapter alloc] initWithFrame:self.frame];
    }
    
    _subview.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    [_subview setUrl:url credential:credential useBasicAuth:useBasicAuth resolution:resolution angle:angle andAspectRatio:aspectRatio];
    [_subview setDelegate:_delegate];
    
    for (UIView *view in self.subviews) {
        [view removeFromSuperview];
    }
    
    [self addSubview:_subview];
}

- (void)play {
    [_subview play];
}

- (void)pause {
    [_subview pause];
}

- (BOOL)isPaused {
    return [_subview isPaused];
}

- (void)stop {
    [_subview setDelegate:nil];
    [_subview stop];
}

@end
