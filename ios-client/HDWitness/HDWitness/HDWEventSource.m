//
//  EventSource.m
//  EventSource
//
//  Created by Neil on 25/07/2013.
//  Copyright (c) 2013 Neil Cowburn. All rights reserved.
//

#import "CKStringUtils.h"
#import "HDWEventSource.h"

#import "HDWLegacyStreamParser.h"
#import "HDWMultipartStreamParser.h"

#import "NSString+Base64.h"
#import "NSString+MD5.h"
#import "NSString+OptixAdditions.h"


@interface HDWEventSource () <NSURLConnectionDelegate, NSURLConnectionDataDelegate> {
    __weak id <HDWEventSourceDelegate> delegate;
    NSURLRequest *eventURLRequest;
    NSURLCredential *_credential;
    NSURLConnection *eventSource;
    NSMutableDictionary *listeners;
    BOOL wasClosed;
    NSObject<HDWStreamParser> *streamParser;
    NSInteger _failCount;
}

@end

@implementation HDWEventSource

- (instancetype)initWithURL:(NSURL *)URL credential:(NSURLCredential *)credential andDelegate:(id <HDWEventSourceDelegate>)delegate_
{
    return [self initWithURLRequest:[NSURLRequest requestWithURL:URL] credential:credential andDelegate:delegate_];
}

- (instancetype)initWithURLRequest:(NSURLRequest *)request credential:(NSURLCredential *)credential andDelegate:(id <HDWEventSourceDelegate>)delegate_
{
    if (self = [super init]) {
        _failCount = 0;
        listeners = [NSMutableDictionary dictionary];
        eventURLRequest = request;
        _credential = credential;
        delegate = delegate_;
        
        [self onOpen:^(Event *event) {
            [delegate eventSourceDidOpen:self];
        }];
        
        [self onMessage:^(Event *event) {
            [delegate eventSource:self didReceiveMessage:event];
        }];
        
        [self onError:^(Event *event) {
            [delegate eventSource:self didFailWithError:event.error];
        }];
    }
    return self;
}

- (void)addEventListener:(NSString *)eventName handler:(EventSourceEventHandler)handler
{
    if (listeners[eventName] == nil) {
        listeners[eventName] = [NSMutableArray array];
    }
    
    [listeners[eventName] addObject:handler];
}

- (void)onMessage:(EventSourceEventHandler)handler
{
    [self addEventListener:MessageEvent handler:handler];
}

- (void)onError:(EventSourceEventHandler)handler
{
    [self addEventListener:ErrorEvent handler:handler];
}

- (void)onOpen:(EventSourceEventHandler)handler
{
    [self addEventListener:OpenEvent handler:handler];
}

- (void)open
{
    wasClosed = NO;
    eventSource = [[NSURLConnection alloc] initWithRequest:eventURLRequest delegate:self startImmediately:YES];
}

- (void)close
{
    wasClosed = YES;
    [eventSource cancel];
}

// ---------------------------------------------------------------------------------------------------------------------

- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    if ([challenge previousFailureCount] == 0) {
        [[challenge sender] useCredential:_credential forAuthenticationChallenge:challenge];
    } else {
        [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];
    }
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    if (httpResponse.statusCode == 200) {
        _failCount = 0;
        
        // Opened
        Event *e = [Event new];
        e.readyState = kEventStateOpen;

        NSString *contentType = httpResponse.allHeaderFields[@"content-type"];
        if ([CKStringUtils string:contentType containsString:@"multipart" ignoreCase:NO]) {
            streamParser = [[HDWMultipartStreamParser alloc] initWithBoundary:@"--ec2boundary"];
        } else {
            streamParser = [[HDWLegacyStreamParser alloc] init];
        }
        NSArray *openHandlers = listeners[OpenEvent];
        for (EventSourceEventHandler handler in openHandlers) {
            dispatch_async(dispatch_get_main_queue(), ^{
                handler(e);
            });
        }
    } else if (httpResponse.statusCode == 401) {
        if (_failCount++ != 0)
            return;
        
        NSString *realm;
        NSString *nonce;
        
        {
            NSString *setCookieString = httpResponse.allHeaderFields[@"Set-Cookie"];
            if ([setCookieString isEmpty])
                return;
            
            NSArray *cookies = [setCookieString componentsSeparatedByString:@", "];
            for (NSString *cookie in cookies) {
                for (NSString *component in [cookie componentsSeparatedByString:@"; "]) {
                    NSArray *nameValue = [component componentsSeparatedByString:@"="];
                    if (nameValue.count == 2) {
                        NSString *name = nameValue[0];
                        NSString *value = nameValue[1];
                        
                        if ([name isEqualToString:@"realm"])
                            realm = value;
                        else if ([name isEqualToString:@"nonce"])
                            nonce = value;
                    }
                }
            }
        }
        
        if ([nonce isEmpty] || [realm isEmpty])
            return;
        
        [delegate setRealm:realm andNonce:nonce];
        [delegate setAuthCookies];
        
        [self close];
        [self open];
    }
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    Event *e = [Event new];
    e.readyState = kEventStateClosed;
    e.error = error;
    
    NSArray *errorHandlers = listeners[ErrorEvent];
    for (EventSourceEventHandler handler in errorHandlers) {
        dispatch_async(dispatch_get_main_queue(), ^{
            handler(e);
        });
    }
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [streamParser addData:data];
    
    NSData *messageData;
    while ((messageData = [streamParser nextMessage]) != nil) {
        Event *e = [Event new];
        e.readyState = kEventStateOpen;
        e.event = @"message";
        e.data = messageData;

        NSArray *messageHandlers = listeners[MessageEvent];
        for (EventSourceEventHandler handler in messageHandlers) {
            dispatch_async(dispatch_get_main_queue(), ^{
                handler(e);
            });
        }
        
        if (e.event != nil) {
            NSArray *namedEventhandlers = listeners[e.event];
            for (EventSourceEventHandler handler in namedEventhandlers) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    handler(e);
                });
            }
        }
    }

#if 0
    __block NSString *eventString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    
    if ([eventString hasSuffix:@"\n\n"]) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
            eventString = [eventString stringByReplacingOccurrencesOfString:@"\n\n" withString:@""];
            NSMutableArray *components = [[eventString componentsSeparatedByString:@"\n"] mutableCopy];
            
            Event *e = [Event new];
            e.readyState = kEventStateOpen;
            
            for (NSString *component in components) {
                NSArray *pairs = [component componentsSeparatedByString:@": "];
                if ([component hasPrefix:@"id"]) {
                    e.id = pairs[1];
                } else if ([component hasPrefix:@"event"]) {
                    e.event = pairs[1];
                } else if ([component hasPrefix:@"data"]) {
                    e.data = pairs[1];
                }
            }
        });
    }
#endif
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    if (wasClosed) {
        return;
    }
    
    if (_failCount < 2)
        return;
    
    Event *e = [Event new];
    e.readyState = kEventStateClosed;
    e.error = [NSError errorWithDomain:@""
                                  code:e.readyState
                              userInfo:@{ NSLocalizedDescriptionKey: @"Connection with the event source was closed." }];
    
    NSArray *errorHandlers = listeners[ErrorEvent];
    for (EventSourceEventHandler handler in errorHandlers) {
        dispatch_async(dispatch_get_main_queue(), ^{
            handler(e);
        });
    }
}

@end

// ---------------------------------------------------------------------------------------------------------------------

@implementation Event

- (NSString *)description
{
    NSString *state = nil;
    switch (self.readyState) {
        case kEventStateConnecting:
            state = @"CONNECTING";
            break;
        case kEventStateOpen:
            state = @"OPEN";
            break;
        case kEventStateClosed:
            state = @"CLOSED";
            break;
    }
    
    return [NSString stringWithFormat:@"<%@: readyState: %@, id: %@; event: %@; data: %@>",
            [self class],
            state,
            self.id,
            self.event,
            self.data];
}

@end

NSString *const MessageEvent = @"message";
NSString *const ErrorEvent = @"error";
NSString *const OpenEvent = @"open";
