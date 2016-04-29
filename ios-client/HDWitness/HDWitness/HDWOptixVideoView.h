//
//  HDWOptixVideoView.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/11/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@class HDWResolution;

@protocol HDWOptixVideoViewDelegate<NSObject>

- (void)onFirstFrameReceived;
- (void)onFrameReceived:(NSTimeInterval)timestamp andFps:(NSInteger)currentFps;
- (void)onConnectionClosed;

@end

@protocol HDWOptixVideoControl<NSObject>

- (void)setDelegate:(id<HDWOptixVideoViewDelegate>)delegate;
- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSNumber *)angle andAspectRatio:(NSNumber *)aspectRatio;
- (void)play;
- (void)pause;
- (void)stop;
- (BOOL)isPaused;

@end

@interface HDWOptixVideoView : UIView<HDWOptixVideoControl>

@end
