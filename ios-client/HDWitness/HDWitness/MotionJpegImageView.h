//
//  MotionJpegImageView.h
//  VideoTest
//
//  Created by Matthew Eagar on 10/3/11.
//  Copyright 2011 ThinkFlood Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#import <UIKit/UIKit.h>

@class HDWResolution;

@protocol MotionJpegViewDelegate <NSObject>

- (void)onFirstFrameReceived;
- (void)onFrameReceived:(NSTimeInterval)timestamp andFps:(NSInteger)currentFps;
- (void)onConnectionClosed;

@end

@class FpsCounter;

@interface MotionJpegImageView : UIImageView {
    
@private
    NSURL *_url;
    NSURLCredential *_credential;
    BOOL _useBasicAuth;
    NSURLConnection *_connection;
    NSMutableData *_receivedData;
    BOOL _allowSelfSignedCertificates;
    BOOL _allowClearTextCredentials;
    FpsCounter *_fpsCounter;
    BOOL _needReconnect;
    BOOL _firstFrameReceived;
    
    NSTimeInterval _lastTimestamp;
    BOOL _buggyIOS5;
}

@property (nonatomic, readwrite, copy) NSURL *url;
@property (nonatomic, readwrite, copy) NSURLCredential *credential;
@property (readonly) BOOL isPlaying;
@property (nonatomic, readwrite, assign) BOOL allowSelfSignedCertificates;
@property (nonatomic, readwrite, assign) BOOL allowClearTextCredentials;
@property (nonatomic, assign) id<MotionJpegViewDelegate> delegate;

- (void)doInit;
- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSInteger)angle andAspectRatio:(double)aspectRatio;

- (void)play;
- (void)pause;
- (void)clear;
- (void)stop;

@end
