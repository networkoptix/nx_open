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

#define END_MARKER_BYTES { 0xFF, 0xD9 }

static NSData *_endMarkerData = nil;

#pragma mark - Private Method Declarations

@interface MotionJpegImageView ()

@end

#pragma mark - Implementation

@implementation MotionJpegImageView

@synthesize url = _url;
@synthesize username = _username;
@synthesize password = _password;
@synthesize allowSelfSignedCertificates = _allowSelfSignedCertificates;
@synthesize allowClearTextCredentials = _allowClearTextCredentials;
@dynamic isPlaying;

- (BOOL)isPlaying {
    return !(_connection == nil);
}

#pragma mark - Initializers

- (id)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    
    if (self) {
        _url = nil;
        _receivedData = nil;
        _username = nil;
        _password = nil;
        _allowSelfSignedCertificates = NO;
        
        if (_endMarkerData == nil) {
            uint8_t endMarker[2] = END_MARKER_BYTES;
            _endMarkerData = [[NSData alloc] initWithBytes:endMarker length:2];
        }
        
        self.contentMode = UIViewContentModeScaleAspectFit;
    }
    
    return self;
}

-(void)awakeFromNib {
    [super awakeFromNib];
    
    if (_endMarkerData == nil) {
        uint8_t endMarker[2] = END_MARKER_BYTES;
        _endMarkerData = [[NSData alloc] initWithBytes:endMarker length:2];
    }
    
    self.contentMode = UIViewContentModeScaleAspectFit;
}

#pragma mark - Overrides

#pragma mark - Public Methods

- (void)play {
    if (_connection) {
        // continue
    }
    else if (_url) {
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:_url];

         NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", _url.user, _url.password];
        [request addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
        
//        [request setValue:@"bRememberMe=1; userLastLogin=admin; passwordLastLogin=admin; user=admin; password=admin; usr=admin; pwd=admin; usrLevel=0; bShowMenu=1" forHTTPHeaderField:@"Cookie"];
        _connection = [[NSURLConnection alloc] initWithRequest:request delegate:self];
    }
}

- (void)pause {
    if (_connection) {
        [_connection cancel];
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
    _receivedData = [[NSMutableData alloc] init];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    [_receivedData appendData:data];
    
    NSRange endRange = [_receivedData rangeOfData:_endMarkerData 
                                          options:0 
                                            range:NSMakeRange(0, _receivedData.length)];
    
    long long endLocation = endRange.location + endRange.length;
    if (_receivedData.length >= endLocation) {
        NSData *imageData = [_receivedData subdataWithRange:NSMakeRange(0, endLocation)];
        UIImage *receivedImage = [UIImage imageWithData:imageData];
        if (receivedImage) {
            self.image = receivedImage;
        }
    }
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
}

-                    (BOOL)connection:(NSURLConnection *)connection 
canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace {
    BOOL allow = NO;
    if ([protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        allow = _allowSelfSignedCertificates;
    }
    else {
        allow = _allowClearTextCredentials;
    }
    
    return allow;
}

-                (void)connection:(NSURLConnection *)connection 
didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if ([challenge previousFailureCount] == 0 &&
        _username && _username.length > 0 &&
        _password && _password.length > 0) {
        NSURLCredential *credentials = 
            [NSURLCredential credentialWithUser:_username
                                       password:_password
                                    persistence:NSURLCredentialPersistenceForSession];
        [[challenge sender] useCredential:credentials
               forAuthenticationChallenge:challenge];
    }
    else {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
    }
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)connection {
    return YES;
}

- (void)connection:(NSURLConnection *)connection 
  didFailWithError:(NSError *)error {
}

#pragma mark - CredentialAlertView Delegate Methods

@end
