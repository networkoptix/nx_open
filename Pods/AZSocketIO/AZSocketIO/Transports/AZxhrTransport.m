//
//  AZxhrTransport.m
//  AZSocketIO
//
//  Created by Patrick Shields on 5/15/12.
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

#import "AZxhrTransport.h"
#import "AFHTTPClient.h"
#import "AFHTTPRequestOperation.h"
#import "AZSocketIOTransportDelegate.h"

@interface AZxhrTransport ()
@property(nonatomic, weak)id<AZSocketIOTransportDelegate> delegate;
@property(nonatomic, readwrite, assign)BOOL connected;
@end

@implementation AZxhrTransport
@synthesize client;
@synthesize secureConnections;
@synthesize delegate;
@synthesize connected;
- (void)connect
{
    [self.client getPath:@""
              parameters:nil
                 success:^(AFHTTPRequestOperation *operation, id responseObject) {
                     self.connected = YES;
                     if ([self.delegate respondsToSelector:@selector(didOpen)]) {
                         [self.delegate didOpen];
                     }                     
                     NSString *responseString = [self stringFromData:responseObject];
                     NSArray *messages = [responseString componentsSeparatedByString:@"\ufffd"];
                     if ([messages count] > 0) {
                         for (NSString *message in messages) {
                             [self.delegate didReceiveMessage:message];
                         }
                     } else {
                         [self.delegate didReceiveMessage:responseString];
                     }                     
                     
                     if (self.connected) {
                         [self connect];
                     }
                 } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
                     [self.delegate didFailWithError:error];
                     if ([self.delegate respondsToSelector:@selector(didClose)]) {
                         [self.delegate didClose];
                     }
                 }];
}
- (void)disconnect
{
    [self.client.operationQueue cancelAllOperations];
    [self.client getPath:@"?disconnect"
              parameters:nil
                 success:^(AFHTTPRequestOperation *operation, id responseObject) {} 
                 failure:^(AFHTTPRequestOperation *operation, NSError *error) {}];
    self.connected = NO;
    if ([self.delegate respondsToSelector:@selector(didClose)]) {
        [self.delegate didClose];
    }
}
- (void)send:(NSString*)msg
{
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:self.client.baseURL];
    request.HTTPMethod = @"POST";
    [request setHTTPBody:[msg dataUsingEncoding:NSUTF8StringEncoding]];
    [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];
    [request setValue:@"Keep-Alive" forHTTPHeaderField:@"Connection"];
    
    AFHTTPRequestOperation *op = [[AFHTTPRequestOperation alloc] initWithRequest:request];
    [op setCompletionBlockWithSuccess:^(AFHTTPRequestOperation *operation, id responseObject) {
        if ([self.delegate respondsToSelector:@selector(didSendMessage)]) {
            [self.delegate didSendMessage];
        }
    } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
        [self.delegate didFailWithError:error];
    }];
    [self.client enqueueHTTPRequestOperation:op];
}
- (id)initWithDelegate:(id<AZSocketIOTransportDelegate>)_delegate secureConnections:(BOOL)_secureConnections
{
    self = [super init];
    if (self) {
        self.connected = NO;
        self.delegate = _delegate;
        self.secureConnections = _secureConnections;
        
        NSString *protocolString = self.secureConnections ? @"https://" : @"http://";
        NSString *urlString = [NSString stringWithFormat:@"%@%@:%@/socket.io/1/xhr-polling/%@", 
                               protocolString, [self.delegate host], [self.delegate port], 
                               [self.delegate sessionId]];
        
        self.client = [[AFHTTPClient alloc] initWithBaseURL:[NSURL URLWithString:urlString]];
        self.client.stringEncoding = NSUTF8StringEncoding;
    }
    return self;
}
- (NSString *)stringFromData:(NSData *)data
{
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}
@end
