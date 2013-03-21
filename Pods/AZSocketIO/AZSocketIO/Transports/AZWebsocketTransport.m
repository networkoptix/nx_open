//
//  AZWebsocketTransport.m
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

#import "AZWebsocketTransport.h"
#import "AZSocketIOTransportDelegate.h"

@interface AZWebsocketTransport ()
@property(nonatomic, weak)id<AZSocketIOTransportDelegate> delegate;
@property(nonatomic, readwrite, assign)BOOL connected;
@end

@implementation AZWebsocketTransport
@synthesize secureConnections;

@synthesize websocket;
@synthesize delegate;
@synthesize connected;

#pragma mark AZSocketIOTransport
- (id)initWithDelegate:(id<AZSocketIOTransportDelegate>)_delegate secureConnections:(BOOL)_secureConnections
{
    self = [super init];
    if (self) {
        self.connected = NO;
        self.delegate = _delegate;
        self.secureConnections = _secureConnections;
        
        NSString *protocolString = self.secureConnections ? @"wss://" : @"ws://";
        NSString *urlString = [NSString stringWithFormat:@"%@%@:%@/socket.io/1/websocket/%@", 
                               protocolString, [self.delegate host], [self.delegate port], 
                               [self.delegate sessionId]];
        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:urlString]];
        self.websocket = [[SRWebSocket alloc] initWithURLRequest:request];
        self.websocket.delegate = self;
    }
    return self;
}
- (void)dealloc
{
    [self disconnect];
}
- (void)connect
{
    [self.websocket open];
}
- (void)send:(NSString *)msg
{
    [self.websocket send:msg];
    if ([self.delegate respondsToSelector:@selector(didSendMessage)]) {
        [self.delegate didSendMessage];
    }
}
- (void)disconnect
{
    self.websocket.delegate = nil;
    [self.websocket close];
    self.websocket = nil;
    [self webSocket:self.websocket didCloseWithCode:0 reason:@"Client requested disconnect" wasClean:YES];
    self.connected = NO;
}

#pragma mark SRWebSocketDelegate
- (void)webSocket:(SRWebSocket *)webSocket didReceiveMessage:(NSString *)message
{
    [self.delegate didReceiveMessage:message];
}
- (void)webSocketDidOpen:(SRWebSocket *)webSocket
{
    self.connected = YES;
    if ([self.delegate respondsToSelector:@selector(didOpen)]) {
        [self.delegate didOpen];
    }
}
- (void)webSocket:(SRWebSocket *)webSocket didCloseWithCode:(NSInteger)code reason:(NSString *)reason wasClean:(BOOL)wasClean
{
    if (!self.connected || wasClean) {
        if ([self.delegate respondsToSelector:@selector(didClose)]) {
            [self.delegate didClose];
        }
    } else { // Socket disconnections can be errors, but with socket.io was clean always seems to be false, so we'll check on our own
        [self webSocket:webSocket didFailWithError:nil];
    }
}
- (void)webSocket:(SRWebSocket *)webSocket didFailWithError:(NSError *)error
{
    self.connected = NO;
    [self.delegate didFailWithError:error];
}
@end
