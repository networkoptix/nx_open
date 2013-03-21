//
//  AZSocketIOTransport.h
//  AZSocketIO
//
//  Created by Patrick Shields on 8/9/11.
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
#import "AZSocketIOTransportDelegate.h"

/**
 The `AZSocketIOTransport` protocol specifies the requirements of a transport that can be utilized by an `AZSocketIO` object.
 */
@protocol AZSocketIOTransport <NSObject>
@required

///---------------------------------------------
/// @name Creating and Configuring the Transport
///---------------------------------------------

/**
 Initializes an object conforming to `AZSocketIOTransport`.
 
 @param delegate The delegate for the transport to use.
 @param secureConnections Determines whether the transport will secure it's connection.
 
 @return The initialized transport.
 */
- (id)initWithDelegate:(id<AZSocketIOTransportDelegate>)delegate secureConnections:(BOOL)secureConnections;

/**
 Determines whether the transport will secure the connection.
 */
@property(nonatomic, assign)BOOL secureConnections;

/**
 Contains the current state of the transport.
 
 @return `YES` if the transport is connected, otherwise `NO`.
 */
@property(nonatomic, readonly, getter = isConnected)BOOL connected;

/**
 Sets the delegate for the transport.
 
 @param delegate A delegate class conforming to `AZSocketIOTransportDelegate`.
 */
- (void)setDelegate:(id<AZSocketIOTransportDelegate>)delegate;

///------------------------------------
/// @name Communicating With the Server
///------------------------------------

/**
 Causes the transport to connect to the socket.io server.
 */
- (void)connect;

/**
 Causes the transport to disconnect
 */
- (void)disconnect;

/**
 Sends a serialized encoded message to the socket.io server.
 
 @param msg A serialized encoded message.
 */
- (void)send:(NSString*)msg;
@end
