//
//  HDWHTTPRequestOperation.m
//  HDWitness
//
//  Created by Ivan Vigasin on 8/27/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWHTTPRequestOperation.h"

#import "NSString+Base64.h"
#import "NSString+MD5.h"
#import "NSString+OptixAdditions.h"

#import "HDWECSConfig.h"

@implementation HDWHTTPRequestOperation {
}

- (id)initWithRequest:(NSURLRequest *)urlRequest andCredential:(NSURLCredential *)credential {
    if (self = [super initWithRequest:urlRequest]) {
        self.credential = credential;
        __weak typeof(self) weakSelf = self;
        [self setWillSendRequestForAuthenticationChallengeBlock:^(NSURLConnection *connection, NSURLAuthenticationChallenge *challenge) {
            if ([challenge previousFailureCount] == 0) {
                weakSelf.realm = challenge.protectionSpace.realm;
                [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
            } else {
                [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];
            }
        }];
    }
    
    return self;
}

+ (void)setAuthCookiesForConfig:(HDWECSConfig *)config {
    if (config.realm == nil)
        return;
    
    NSString *ha1 = [[NSString stringWithFormat:@"%@:%@:%@", config.login, config.realm, config.password] MD5Digest];
    NSString *ha2 = [[NSString stringWithFormat:@"%@:%@", @"GET", @""] MD5Digest];
    NSString *nonce = [NSString stringWithFormat:@"%llX", (int64_t)(([[NSDate date] timeIntervalSince1970] + config.timeDiff) * 1000000LL)];
    NSString *authDigest = [[NSString stringWithFormat:@"%@:%@:%@", ha1, nonce, ha2] MD5Digest];
    NSString *auth = [[NSString stringWithFormat:@"%@:%@:%@", config.login, nonce, authDigest] base64Encode];
    
    NSDictionary *cookies = @{
                              @"Authorization": @"Digest",
                              @"nonce" : nonce,
                              @"realm": config.realm,
                              @"auth": auth};
    
    [cookies enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        NSDictionary *properties = [NSDictionary dictionaryWithObjectsAndKeys:
                                    config.host, NSHTTPCookieDomain,
                                    @"/", NSHTTPCookiePath,
                                    key, NSHTTPCookieName,
                                    [obj stringByAddingPercentEscapesUsingEncoding:NSASCIIStringEncoding], NSHTTPCookieValue,
                                    @"FALSE", NSHTTPCookieDiscard,
                                    nil];
        [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookie:[NSHTTPCookie cookieWithProperties:properties]];
    }];
}

@end
