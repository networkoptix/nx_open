//
//  AZSocketIO.h
//  AZSocketIO
//
//  Created by Patrick Shields on 4/6/12.
//  Copyright 2012 Patrick Shields
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#import <Foundation/Foundation.h>
#import "AZsocketIOTransportDelegate.h"

@protocol AZSocketIOTransport;

#define AZDOMAIN @"AZSocketIO"

typedef void (^MessageRecievedBlock)(id data);
typedef void (^EventRecievedBlock)(NSString *eventName, id data);
typedef void (^ConnectedBlock)();
typedef void (^DisconnectedBlock)();
typedef void (^ErrorBlock)(NSError *error);

typedef void (^ACKCallback)();
typedef void (^ACKCallbackWithArgs)(NSArray *args);

typedef enum {
    az_socket_connected,
    az_socket_connecting,
    az_socket_not_connected
} AZSocketIOState;

/**
 `AZSocketIO` provides a mechanism for connecting to and interacting with a socket.io compliant server. It maintains the actual transport connection and provides facilities for sending all types of messages.
 */
@interface AZSocketIO : NSObject <AZSocketIOTransportDelegate>
/**
 The request object to the socket.io server
 */
@property(nonatomic, strong, readonly)NSURLRequest *request;
/**
 Contains the current state of the connection.
 
 @warning This property may conflict with the state of the transport during state changes.
 */
@property(nonatomic, readonly)AZSocketIOState state;
/**
 The set of transports the client wishes to use. Defaults to "websocket" and "xhr-polling".
 */
@property(nonatomic, strong)NSMutableSet *transports;
/**
 The currently active transport, if one exists.
 */
@property(nonatomic, strong)id<AZSocketIOTransport> transport;

/**
 This block will be called on the reception of any non-protocol message.
 */
@property(nonatomic, copy)MessageRecievedBlock messageRecievedBlock;
/**
 This block will be called on the reception of any event.
 */
@property(nonatomic, copy)EventRecievedBlock eventRecievedBlock;
/**
 This block will be called after the instance has disconnected
 */
@property(nonatomic, copy)DisconnectedBlock disconnectedBlock;
/**
 This block will be called when an error is reported by the socket.io server or the connection becomes unusable.
 */
@property(nonatomic, copy)ErrorBlock errorBlock;

///----------------------------------------------------
/// @name Creating and Connecting to a Socket.io Server
///----------------------------------------------------

/**
 Initializes an `AZSocketIO` object with the specified host and port.
 
 This is the designated initializer. It will not create a connection to the server.
 
 @param host The hostname of socket.io server.
 @param port The port the socket.io server is running on.
 @param secureConnections Determines whether SSL encryption is used when possible
 
 @return the initialized client
 */
- (id)initWithRequest:(NSURLRequest*)request;

/**
 Connects to the socket.io server.
 
 @param success A block object that will be executed after the completion of handshake.
 @param failure A block object that will be executed when an error is reported by the socket.io server or the connection becomes unusable.
 */
- (void)connectWithSuccess:(void (^)())success andFailure:(void (^)(NSError *error))failure;
/**
 Disconnects from the socket.io server
 */
- (void)disconnect;

///-----------------------
/// @name Sending messages
///-----------------------

/**
 Sends a normal message to the socket.io server.
 
 @param data The data to be sent to the socket.io server. If this data is not an `NSString`, the data will be encoded as JSON.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)send:(id)data error:(NSError * __autoreleasing *)error;

/**
 Sends a normal message to the socket.io server.
 
 This functions identically to the socket.io javascript client. For discussion of that, see the [socket.io readme](https://github.com/learnboost/socket.io#getting-acknowledgements).
 
 @param data The data to be sent to the socket.io server. If this data is not an `NSString`, the data will be encoded as JSON.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 @param callback A block that will be executed using the ACK args from the socket.io server.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)send:(id)data error:(NSError *__autoreleasing *)error ackWithArgs:(void (^)(NSArray *data))callback;

/**
 Sends a normal message to the socket.io server.
 
 This functions identically to the socket.io javascript client. For discussion of that, see the [socket.io readme](https://github.com/learnboost/socket.io#getting-acknowledgements).
 
 @param data The data to be sent to the socket.io server. If this data is not an `NSString`, the data will be encoded as JSON.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 @param callback A block that will be executed using the ACK from the socket.io server.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)send:(id)data error:(NSError *__autoreleasing *)error ack:(void (^)())callback;

/**
 Emits a namespaced message to the socket.io server.
 
 @param name The name of the event.
 @param args The arguements to emit with the event.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)emit:(NSString *)name args:(id)args error:(NSError * __autoreleasing *)error;

/**
 Emits a namespaced message to the socket.io server.
 
 This functions identically to the socket.io javascript client. For discussion of that, see the [socket.io readme](https://github.com/learnboost/socket.io#getting-acknowledgements).
 
 @param name The name of the event.
 @param args The arguements to emit with the event.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 @param callback A block that will be executed using the ACK args from the socket.io server.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)emit:(NSString *)name args:(id)args error:(NSError *__autoreleasing *)error ackWithArgs:(void (^)(NSArray *data))callback;

/**
 Emits a namespaced message to the socket.io server.
 
 This functions identically to the socket.io javascript client. For discussion of that, see the [socket.io readme](https://github.com/learnboost/socket.io#getting-acknowledgements).
 
 @param name The name of the event.
 @param args The arguements to emit with the event.
 @param error If there is a problem encoding the message, upon return contains an instance of NSError that describes the problem.
 @param callback A block that will be executed using the ACK from the socket.io server.
 
 @return `YES` if the message was dispatched immediately, `NO` if it was queued.
 */
- (BOOL)emit:(NSString *)name args:(id)args error:(NSError *__autoreleasing *)error ack:(void (^)())callback;

///-------------------------------------
/// @name Routing Events From the Server
///-------------------------------------

/**
 Sets the callback for a particular event
 
 For most users, this will be the most common way to handle messages.
 
 @param name The name of the event.
 @param block A block object that will be called when an event with this name is recieved. The block has no return value and takes two arguements: the name of the event and the arguements sent with the event.
 
 @warning A single event can have many registered callback blocks. Adding a new callback does not implicitly remove the existing callback. To remove existing callbacks, see `removeCallbackForEvent:callback:` or `removeCallbacksForEvent:`.
 */
- (void)addCallbackForEventName:(NSString *)name callback:(void (^)(NSString *eventName, id data))block;

/**
 Removes a single callback for a particular event
 
 @param name The name of the event.
 @param block A block object that is currently registered with this event.
 
 @return `YES` if there are no remaining callbacks for this event, `NO` if other callbacks remain
 */
- (BOOL)removeCallbackForEvent:(NSString *)name callback:(void (^)(NSString *eventName, id data))block;

/**
 Removes all callbacks for a particular event
 
 @param name The name of the event.
 
 @return The number of callbacks that were removed.
 */
- (NSInteger)removeCallbacksForEvent:(NSString *)name;

/*!
 @method setValue:forHTTPHeaderField:
 @abstract Sets the value of the given HTTP header field.
 @discussion If a value was previously set for the given header
 field, that value is replaced with the given value. Note that, in
 keeping with the HTTP RFC, HTTP header field names are
 case-insensitive.
 @param value the header field value.
 @param field the header field name (case-insensitive).
 */
- (void)setValue:(NSString *)value forHTTPHeaderField:(NSString *)field;

///-------------------------------------
/// @name Reconnecting
///-------------------------------------
/**
 Determines whether AZSocketIO will try to reconnect. Defaults to 'YES'.
 */
@property(nonatomic, assign, getter = shouldReconnect)BOOL reconnect;
/**
 The initial delay, in seconds, before reconnecting. Defaults to '0.5'.
 */
@property(nonatomic, assign)NSTimeInterval reconnectionDelay;
/**
 The maximum delay, in seconds, before reconnecting. After the delay hits this ceiling, reconnection attempts will stop. Defaults to 'MAX_FLOAT'.
 */
@property(nonatomic, assign)NSTimeInterval reconnectionLimit;
/**
 The maximum number of reconnection attempts. Defaults to '10'.
 */
@property(nonatomic, assign)NSUInteger maxReconnectionAttempts;

#pragma mark overridden setters
- (void)setMessageRecievedBlock:(void (^)(id data))messageRecievedBlock;
- (void)setEventRecievedBlock:(void (^)(NSString *eventName, id data))eventRecievedBlock;
- (void)setDisconnectedBlock:(void (^)())disconnectedBlock;
- (void)setErrorBlock:(void (^)(NSError *error))errorBlock;
@end
