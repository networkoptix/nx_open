//
//  EventSource.h
//  EventSource
//
//  Created by Neil on 25/07/2013.
//  Copyright (c) 2013 Neil Cowburn. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef enum {
    kEventStateConnecting = 0,
    kEventStateOpen = 1,
    kEventStateClosed = 2,
} EventState;

// ---------------------------------------------------------------------------------------------------------------------

/// Describes an Event received from an EventSource
@interface Event : NSObject

/// The Event ID
@property (nonatomic, strong) id id;
/// The name of the Event
@property (nonatomic, strong) NSString *event;
/// The data received from the EventSource
@property (nonatomic, strong) NSData *data;

/// The current state of the connection to the EventSource
@property (nonatomic, assign) EventState readyState;
/// Provides details of any errors with the connection to the EventSource
@property (nonatomic, strong) NSError *error;

@end

// ---------------------------------------------------------------------------------------------------------------------

typedef void (^EventSourceEventHandler)(Event *event);

// ---------------------------------------------------------------------------------------------------------------------

#pragma mark - EventSourceDelegate

@protocol HDWEventSourceDelegate;

#pragma mark - EventSource

/// Connect to and receive Server-Sent Events (SSEs).
@interface HDWEventSource : NSObject

/// Creates a new instance of EventSource with the specified URL.
///
/// @param URL The URL of the EventSource.
- (instancetype)initWithURL:(NSURL *)URL credential:(NSURLCredential *)credential andDelegate:(id <HDWEventSourceDelegate>)delegate;

- (instancetype)initWithURLRequest:(NSURLRequest *)request credential:(NSURLCredential *)credential andDelegate:(id <HDWEventSourceDelegate>)delegate;

/// Registers an event handler for the Message event.
///
/// @param handler The handler for the Message event.
- (void)onMessage:(EventSourceEventHandler)handler;

/// Registers an event handler for the Error event.
///
/// @param handler The handler for the Error event.
- (void)onError:(EventSourceEventHandler)handler;

/// Registers an event handler for the Open event.
///
/// @param handler The handler for the Open event.
- (void)onOpen:(EventSourceEventHandler)handler;

/// Registers an event handler for a named event.
///
/// @param eventName The name of the event you registered.
/// @param handler The handler for the Message event.
- (void)addEventListener:(NSString *)eventName handler:(EventSourceEventHandler)handler;

- (void)open;
- (void)close;

@end

#pragma mark - EventSourceDelegate

@protocol HDWEventSourceDelegate <NSObject>

- (void)eventSource:(HDWEventSource *)eventSource didReceiveMessage:(Event *)event;

- (void)setRealm:(NSString *)realm andNonce:(NSString *)nonce;
- (void)setAuthCookies;

@optional
- (void)eventSourceDidOpen:(HDWEventSource *)eventSource;
- (void)eventSource:(HDWEventSource *)eventSource didFailWithError:(NSError *)error;
@end

// ---------------------------------------------------------------------------------------------------------------------

extern NSString *const MessageEvent;
extern NSString *const ErrorEvent;
extern NSString *const OpenEvent;
