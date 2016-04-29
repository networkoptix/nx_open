//
//  MotionJpegImageView.mm
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

#import "MotionJpegImageView.h"
#import "NSString+Base64.h"

#pragma mark - Constants

#define RETRY_DELAY 5.0
#define FPS_MEASURE_SECONDS 5
#define END_MARKER_BYTES { 0xFF, 0xD9 }

static NSData *_endMarkerData = nil;

#pragma mark - Private Method Declarations

@interface MotionJpegImageView ()

@end

@interface FpsCounter : NSObject  {
    NSUInteger _interval;
    NSMutableArray *_timestampQueue;
}

+(FpsCounter*) fpsCounterWithInterval: (NSUInteger)interval;

-(id) initWithInterval: (NSUInteger)interval;

-(void) postDisplay;
-(NSUInteger) currentFps;

@end

#pragma mark - Implementation

@implementation FpsCounter

+(FpsCounter*) fpsCounterWithInterval: (NSUInteger)interval {
    return [[FpsCounter alloc] initWithInterval:interval];
}

-(FpsCounter*) initWithInterval: (NSUInteger)interval {
    self = [super init];
    if (self) {
        _interval = interval;
        _timestampQueue = [NSMutableArray array];
    }
    
    return self;
}

-(void) clearOld: (double)currentTime {
    NSIndexSet *indexesToRemove = [_timestampQueue indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
        if (currentTime - ((NSNumber*)obj).doubleValue > _interval)
            return YES;
        else {
            *stop = YES;
            return NO;
        }
    }];
    [_timestampQueue removeObjectsAtIndexes:indexesToRemove];
}

-(void) postDisplay {
    double currentTime = CFAbsoluteTimeGetCurrent();
    [self clearOld:currentTime];
    [_timestampQueue addObject:@(currentTime)];
}

-(NSUInteger) currentFps {
    if (_timestampQueue.count == 0)
        return 0;
    
    double currentTime = CFAbsoluteTimeGetCurrent();
    [self clearOld:currentTime];
    
    if (_timestampQueue.count == 0)
        return 0;
    
    double first = [_timestampQueue[0] doubleValue];
    
    double measureInterval = currentTime - first;
    if (measureInterval < 0.5)
        return 0;
    
    return round(_timestampQueue.count / measureInterval);
}

@end

@implementation MotionJpegImageView

@synthesize url = _url;
@synthesize allowSelfSignedCertificates = _allowSelfSignedCertificates;
@synthesize allowClearTextCredentials = _allowClearTextCredentials;
@dynamic isPlaying;

- (BOOL)isPlaying {
    return !(_connection == nil);
}

#pragma mark - Initializers

- (void) doInit {
    _buggyIOS5 = NO;
    
    NSArray *versionCompatibility = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];
    if (5 == [versionCompatibility[0] intValue] ) { /// iOS5 is installed
        _buggyIOS5 = YES;
    }
    
    _url = nil;
    _receivedData = nil;
    _allowSelfSignedCertificates = YES;
    _allowClearTextCredentials = YES;
    _needReconnect = NO;
    
    if (_fpsCounter == nil)
        _fpsCounter = [FpsCounter fpsCounterWithInterval:FPS_MEASURE_SECONDS];
    
    if (_endMarkerData == nil) {
        uint8_t endMarker[2] = END_MARKER_BYTES;
        _endMarkerData = [[NSData alloc] initWithBytes:endMarker length:2];
    }
    
    self.contentMode = UIViewContentModeScaleAspectFit;
}

- (id) init {
    self = [super init];
    
    if (self) {
        [self doInit];
    }
    
    return self;
}

- (id) initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    
    if (self) {
        [self doInit];
    }
    
    return self;
}

- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSInteger)angle andAspectRatio:(double)aspectRatio {
    [self doInit];
    
    if (_url != url) {
        _needReconnect = YES;
    }
    
    _url = url;
    _credential = credential;
    _useBasicAuth = useBasicAuth;
}

-(void)awakeFromNib {
    [super awakeFromNib];
    
    if (_endMarkerData == nil) {
        uint8_t endMarker[2] = END_MARKER_BYTES;
        _endMarkerData = [[NSData alloc] initWithBytes:endMarker length:2];
    }

    _fpsCounter = [FpsCounter fpsCounterWithInterval:FPS_MEASURE_SECONDS];
    self.contentMode = UIViewContentModeScaleAspectFit;
}

#pragma mark - Overrides

#pragma mark - Public Methods

- (void)play {
    if (_needReconnect) {
        [self stop];
        _needReconnect = NO;
    }
    
    if (_connection) {
        // continue
    }
    else if (_url) {
        _firstFrameReceived = NO;
        
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:_url];

        if (_useBasicAuth) {
            NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", _credential.user, _credential.password];
            [request addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
        }
        
        _connection = [[NSURLConnection alloc] initWithRequest:request delegate:self];
    }
}

- (void)pause {
    if (_connection) {
        [_connection cancel];
        _connection = nil;
    }
}

- (void)clear {
    self.image = nil;
}

- (void)stop {
    [self pause];
    [self clear];
}

#pragma mark - Private Methods

#pragma mark - NSURLConnection Delegate Methods

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)response;
    NSTimeInterval timeSinceEpoch = 0.0;
    if (!_buggyIOS5) {
        timeSinceEpoch = [httpResponse.allHeaderFields[@"x-Content-Timestamp"] longLongValue] / 1e+6;
    } else {
        NSString *contentType = httpResponse.allHeaderFields[@"Content-Type"];
        // Length of "image/jpeg;ts="
        if (contentType.length > 14)
            timeSinceEpoch = [[contentType substringFromIndex:14] longLongValue] / 1e+6;
    }
    
    _lastTimestamp = timeSinceEpoch;
    _receivedData = [[NSMutableData alloc] init];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    if (!_delegate) {
        [connection cancel];
        return;
    }
    
    [_receivedData appendData:data];
    
    NSRange endRange = [_receivedData rangeOfData:_endMarkerData 
                                          options:0 
                                            range:NSMakeRange(0, _receivedData.length)];
    
    int64_t endLocation = endRange.location + endRange.length;
    if (_receivedData.length >= endLocation) {
        NSData *imageData = [_receivedData subdataWithRange:NSMakeRange(0, endLocation)];
        
        UIImage *receivedImage = [UIImage imageWithData:imageData];
        if (receivedImage) {
            [_fpsCounter postDisplay];
            
            if (!_firstFrameReceived) {
                [_delegate onFirstFrameReceived];
                _firstFrameReceived = YES;
            }
            
            self.image = receivedImage;
            [_delegate onFrameReceived:_lastTimestamp andFps:[_fpsCounter currentFps]];
        }
    }
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    [_delegate onConnectionClosed];
}

- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace {
    BOOL allow = NO;
    if ([protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        allow = _allowSelfSignedCertificates;
    }
    else {
        allow = _allowClearTextCredentials;
    }
    
    return allow;
}

- (void)connection:(NSURLConnection *)connection
didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if ([challenge previousFailureCount] == 0) {
        [[challenge sender] useCredential:_credential
               forAuthenticationChallenge:challenge];
    }
    else {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
    }
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection {
    return NO;
}

- (void)connection:(NSURLConnection *)connection 
  didFailWithError:(NSError *)error {
    [self stop];
    _connection = nil;
    if (self.delegate)
        [self performSelector:@selector(play) withObject:nil afterDelay:RETRY_DELAY];
}

@end
