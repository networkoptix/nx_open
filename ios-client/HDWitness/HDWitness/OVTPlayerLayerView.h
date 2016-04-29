//
//  OVTPlayerLayerView.h
//  VideoTest
//
//  Created by Ivan Vigasin on 4/8/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

@class AVPlayerLayer;
@class AVPlayer;
@class AVPlayerItem;
@class HDWResolution;

//@class GPUImageMovie;
//@class GPUImageView;

@protocol OVTPlayerLayerViewDelegate <NSObject>

- (void)onFirstFrameReceived;
- (void)onFrameReceived:(NSTimeInterval)timestamp andFps:(NSInteger)currentFps;
- (void)onConnectionClosed;

@end

@interface OVTPlayerLayerView : UIView

- (AVPlayerLayer *)playerLayer;
- (void)setVideoFillMode:(NSString *)fillMode;

- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSNumber *)angle andAspectRatio:(NSNumber *)aspectRatio;

- (void)play;
- (void)pause;
- (void)stop;
- (BOOL)isPaused;

//- (void)setPlayer:(AVPlayer*)player;

@property(nonatomic) AVPlayer *player;
@property(nonatomic) AVPlayerItem *playerItem;
//@property(nonatomic) GPUImageView *videoView;
//@property(nonatomic) GPUImageMovie *movieFile;
@property(nonatomic,weak) id<OVTPlayerLayerViewDelegate> delegate;

@end
