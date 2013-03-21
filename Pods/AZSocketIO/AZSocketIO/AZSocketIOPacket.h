//
//  AZSocketIOPacket.h
//  AZSocketIO
//
//  Created by Patrick Shields on 4/7/12.
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

typedef enum {
    DISCONNECT,
    CONNECT,
    HEARTBEAT,
    MESSAGE,
    JSON_MESSAGE,
    EVENT,
    ACK,
    ERROR,
    NOOP
} MESSAGE_TYPE;

/**
 `AZSocketIOPacket` is an object that represents an encoded message that can be read by the socket.io server.
 */
@interface AZSocketIOPacket : NSObject
/**
 The type of the message.
 */
@property(nonatomic, assign)MESSAGE_TYPE type;

/**
 The id of the message. This is used for ack'ing packets.
 */
@property(nonatomic, strong)NSString *Id;

/**
 Determines whether the server will ACK a packet. If 'YES', ACK packet will be sent.
 */
@property(nonatomic, assign)BOOL ack;

/**
 The endpoint for the packet. Currently unused.
 */
@property(nonatomic, strong)NSString *endpoint;

/**
 The data to be appended to the packet. This should be preformated to match the type.
 */
@property(nonatomic, strong)NSString *data;

/**
 Initializes a packet using a serialized representation.
 
 @param packetString A serialized packet.
 
 @return the initialized packet.
 */
- (id)initWithString:(NSString *)packetString;

/**
 Serializes a packet so that it can be written to the wire.
 
 @return A string containing the serialized packet data.
 */
- (NSString *)encode;
@end

@interface AZSocketIOACKMessage : NSObject
@property(nonatomic, strong)NSString *messageId;
@property(nonatomic, strong)NSArray *args;
- (id)initWithPacket:(AZSocketIOPacket *)packet;
@end
