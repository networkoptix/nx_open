// Generated by the protocol buffer compiler.  DO NOT EDIT!

#import "Server.pb.h"

@implementation ServerRoot
static id<PBExtensionField> Server_resource = nil;
static PBExtensionRegistry* extensionRegistry = nil;
+ (PBExtensionRegistry*) extensionRegistry {
  return extensionRegistry;
}

+ (void) initialize {
  if (self == [ServerRoot class]) {
    Server_resource =
      [PBConcreteExtensionField extensionWithType:PBExtensionTypeMessage
                                     extendedClass:[Resource class]
                                       fieldNumber:101
                                      defaultValue:[Server defaultInstance]
                               messageOrGroupClass:[Server class]
                                        isRepeated:NO
                                          isPacked:NO
                            isMessageSetWireFormat:NO];
    PBMutableExtensionRegistry* registry = [PBMutableExtensionRegistry registry];
    [self registerAllExtensions:registry];
    [ResourceRoot registerAllExtensions:registry];
    extensionRegistry = registry;
  }
}
+ (void) registerAllExtensions:(PBMutableExtensionRegistry*) registry {
  [registry addExtension:Server_resource];
}
@end

@interface Server ()
@property (strong) NSString* apiUrl;
@property (strong) NSString* netAddrList;
@property BOOL reserve;
@property (strong) NSMutableArray * storageArray;
@property int32_t panicMode;
@property (strong) NSString* streamingUrl;
@property (strong) NSString* version;
@end

@implementation Server

- (BOOL) hasApiUrl {
  return !!hasApiUrl_;
}
- (void) setHasApiUrl:(BOOL) value {
  hasApiUrl_ = !!value;
}
@synthesize apiUrl;
- (BOOL) hasNetAddrList {
  return !!hasNetAddrList_;
}
- (void) setHasNetAddrList:(BOOL) value {
  hasNetAddrList_ = !!value;
}
@synthesize netAddrList;
- (BOOL) hasReserve {
  return !!hasReserve_;
}
- (void) setHasReserve:(BOOL) value {
  hasReserve_ = !!value;
}
- (BOOL) reserve {
  return !!reserve_;
}
- (void) setReserve:(BOOL) value {
  reserve_ = !!value;
}
@synthesize storageArray;
@dynamic storage;
- (BOOL) hasPanicMode {
  return !!hasPanicMode_;
}
- (void) setHasPanicMode:(BOOL) value {
  hasPanicMode_ = !!value;
}
@synthesize panicMode;
- (BOOL) hasStreamingUrl {
  return !!hasStreamingUrl_;
}
- (void) setHasStreamingUrl:(BOOL) value {
  hasStreamingUrl_ = !!value;
}
@synthesize streamingUrl;
- (BOOL) hasVersion {
  return !!hasVersion_;
}
- (void) setHasVersion:(BOOL) value {
  hasVersion_ = !!value;
}
@synthesize version;
- (id) init {
  if ((self = [super init])) {
    self.apiUrl = @"";
    self.netAddrList = @"";
    self.reserve = NO;
    self.panicMode = 0;
    self.streamingUrl = @"";
    self.version = @"";
  }
  return self;
}
+ (id<PBExtensionField>) resource {
  return Server_resource;
}
static Server* defaultServerInstance = nil;
+ (void) initialize {
  if (self == [Server class]) {
    defaultServerInstance = [[Server alloc] init];
  }
}
+ (Server*) defaultInstance {
  return defaultServerInstance;
}
- (Server*) defaultInstance {
  return defaultServerInstance;
}
- (NSArray *)storage {
  return storageArray;
}
- (Server_Storage*)storageAtIndex:(NSUInteger)index {
  return [storageArray objectAtIndex:index];
}
- (BOOL) isInitialized {
  if (!self.hasApiUrl) {
    return NO;
  }
  for (Server_Storage* element in self.storage) {
    if (!element.isInitialized) {
      return NO;
    }
  }
  return YES;
}
- (void) writeToCodedOutputStream:(PBCodedOutputStream*) output {
  if (self.hasApiUrl) {
    [output writeString:1 value:self.apiUrl];
  }
  if (self.hasNetAddrList) {
    [output writeString:2 value:self.netAddrList];
  }
  if (self.hasReserve) {
    [output writeBool:3 value:self.reserve];
  }
  for (Server_Storage *element in self.storageArray) {
    [output writeMessage:4 value:element];
  }
  if (self.hasPanicMode) {
    [output writeInt32:5 value:self.panicMode];
  }
  if (self.hasStreamingUrl) {
    [output writeString:6 value:self.streamingUrl];
  }
  if (self.hasVersion) {
    [output writeString:7 value:self.version];
  }
  [self.unknownFields writeToCodedOutputStream:output];
}
- (int32_t) serializedSize {
  int32_t size = memoizedSerializedSize;
  if (size != -1) {
    return size;
  }

  size = 0;
  if (self.hasApiUrl) {
    size += computeStringSize(1, self.apiUrl);
  }
  if (self.hasNetAddrList) {
    size += computeStringSize(2, self.netAddrList);
  }
  if (self.hasReserve) {
    size += computeBoolSize(3, self.reserve);
  }
  for (Server_Storage *element in self.storageArray) {
    size += computeMessageSize(4, element);
  }
  if (self.hasPanicMode) {
    size += computeInt32Size(5, self.panicMode);
  }
  if (self.hasStreamingUrl) {
    size += computeStringSize(6, self.streamingUrl);
  }
  if (self.hasVersion) {
    size += computeStringSize(7, self.version);
  }
  size += self.unknownFields.serializedSize;
  memoizedSerializedSize = size;
  return size;
}
+ (Server*) parseFromData:(NSData*) data {
  return (Server*)[[[Server builder] mergeFromData:data] build];
}
+ (Server*) parseFromData:(NSData*) data extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server*)[[[Server builder] mergeFromData:data extensionRegistry:extensionRegistry] build];
}
+ (Server*) parseFromInputStream:(NSInputStream*) input {
  return (Server*)[[[Server builder] mergeFromInputStream:input] build];
}
+ (Server*) parseFromInputStream:(NSInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server*)[[[Server builder] mergeFromInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Server*) parseFromCodedInputStream:(PBCodedInputStream*) input {
  return (Server*)[[[Server builder] mergeFromCodedInputStream:input] build];
}
+ (Server*) parseFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server*)[[[Server builder] mergeFromCodedInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Server_Builder*) builder {
  return [[Server_Builder alloc] init];
}
+ (Server_Builder*) builderWithPrototype:(Server*) prototype {
  return [[Server builder] mergeFrom:prototype];
}
- (Server_Builder*) builder {
  return [Server builder];
}
- (Server_Builder*) toBuilder {
  return [Server builderWithPrototype:self];
}
- (void) writeDescriptionTo:(NSMutableString*) output withIndent:(NSString*) indent {
  NSUInteger listCount = 0;
  if (self.hasApiUrl) {
    [output appendFormat:@"%@%@: %@\n", indent, @"apiUrl", self.apiUrl];
  }
  if (self.hasNetAddrList) {
    [output appendFormat:@"%@%@: %@\n", indent, @"netAddrList", self.netAddrList];
  }
  if (self.hasReserve) {
    [output appendFormat:@"%@%@: %@\n", indent, @"reserve", [NSNumber numberWithBool:self.reserve]];
  }
  for (Server_Storage* element in self.storageArray) {
    [output appendFormat:@"%@%@ {\n", indent, @"storage"];
    [element writeDescriptionTo:output
                     withIndent:[NSString stringWithFormat:@"%@  ", indent]];
    [output appendFormat:@"%@}\n", indent];
  }
  if (self.hasPanicMode) {
    [output appendFormat:@"%@%@: %@\n", indent, @"panicMode", [NSNumber numberWithInt:self.panicMode]];
  }
  if (self.hasStreamingUrl) {
    [output appendFormat:@"%@%@: %@\n", indent, @"streamingUrl", self.streamingUrl];
  }
  if (self.hasVersion) {
    [output appendFormat:@"%@%@: %@\n", indent, @"version", self.version];
  }
  [self.unknownFields writeDescriptionTo:output withIndent:indent];
}
- (BOOL) isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (![other isKindOfClass:[Server class]]) {
    return NO;
  }
  Server *otherMessage = other;
  return
      self.hasApiUrl == otherMessage.hasApiUrl &&
      (!self.hasApiUrl || [self.apiUrl isEqual:otherMessage.apiUrl]) &&
      self.hasNetAddrList == otherMessage.hasNetAddrList &&
      (!self.hasNetAddrList || [self.netAddrList isEqual:otherMessage.netAddrList]) &&
      self.hasReserve == otherMessage.hasReserve &&
      (!self.hasReserve || self.reserve == otherMessage.reserve) &&
      [self.storageArray isEqualToArray:otherMessage.storageArray] &&
      self.hasPanicMode == otherMessage.hasPanicMode &&
      (!self.hasPanicMode || self.panicMode == otherMessage.panicMode) &&
      self.hasStreamingUrl == otherMessage.hasStreamingUrl &&
      (!self.hasStreamingUrl || [self.streamingUrl isEqual:otherMessage.streamingUrl]) &&
      self.hasVersion == otherMessage.hasVersion &&
      (!self.hasVersion || [self.version isEqual:otherMessage.version]) &&
      (self.unknownFields == otherMessage.unknownFields || (self.unknownFields != nil && [self.unknownFields isEqual:otherMessage.unknownFields]));
}
- (NSUInteger) hash {
  NSUInteger hashCode = 7;
  NSUInteger listCount = 0;
  if (self.hasApiUrl) {
    hashCode = hashCode * 31 + [self.apiUrl hash];
  }
  if (self.hasNetAddrList) {
    hashCode = hashCode * 31 + [self.netAddrList hash];
  }
  if (self.hasReserve) {
    hashCode = hashCode * 31 + [[NSNumber numberWithBool:self.reserve] hash];
  }
  for (Server_Storage* element in self.storageArray) {
    hashCode = hashCode * 31 + [element hash];
  }
  if (self.hasPanicMode) {
    hashCode = hashCode * 31 + [[NSNumber numberWithInt:self.panicMode] hash];
  }
  if (self.hasStreamingUrl) {
    hashCode = hashCode * 31 + [self.streamingUrl hash];
  }
  if (self.hasVersion) {
    hashCode = hashCode * 31 + [self.version hash];
  }
  hashCode = hashCode * 31 + [self.unknownFields hash];
  return hashCode;
}
@end

@interface Server_Storage ()
@property int32_t id;
@property (strong) NSString* name;
@property (strong) NSString* url;
@property int64_t spaceLimit;
@property BOOL usedForWriting;
@end

@implementation Server_Storage

- (BOOL) hasId {
  return !!hasId_;
}
- (void) setHasId:(BOOL) value {
  hasId_ = !!value;
}
@synthesize id;
- (BOOL) hasName {
  return !!hasName_;
}
- (void) setHasName:(BOOL) value {
  hasName_ = !!value;
}
@synthesize name;
- (BOOL) hasUrl {
  return !!hasUrl_;
}
- (void) setHasUrl:(BOOL) value {
  hasUrl_ = !!value;
}
@synthesize url;
- (BOOL) hasSpaceLimit {
  return !!hasSpaceLimit_;
}
- (void) setHasSpaceLimit:(BOOL) value {
  hasSpaceLimit_ = !!value;
}
@synthesize spaceLimit;
- (BOOL) hasUsedForWriting {
  return !!hasUsedForWriting_;
}
- (void) setHasUsedForWriting:(BOOL) value {
  hasUsedForWriting_ = !!value;
}
- (BOOL) usedForWriting {
  return !!usedForWriting_;
}
- (void) setUsedForWriting:(BOOL) value {
  usedForWriting_ = !!value;
}
- (id) init {
  if ((self = [super init])) {
    self.id = 0;
    self.name = @"";
    self.url = @"";
    self.spaceLimit = 0L;
    self.usedForWriting = NO;
  }
  return self;
}
static Server_Storage* defaultServer_StorageInstance = nil;
+ (void) initialize {
  if (self == [Server_Storage class]) {
    defaultServer_StorageInstance = [[Server_Storage alloc] init];
  }
}
+ (Server_Storage*) defaultInstance {
  return defaultServer_StorageInstance;
}
- (Server_Storage*) defaultInstance {
  return defaultServer_StorageInstance;
}
- (BOOL) isInitialized {
  if (!self.hasName) {
    return NO;
  }
  if (!self.hasUrl) {
    return NO;
  }
  if (!self.hasSpaceLimit) {
    return NO;
  }
  return YES;
}
- (void) writeToCodedOutputStream:(PBCodedOutputStream*) output {
  if (self.hasId) {
    [output writeInt32:1 value:self.id];
  }
  if (self.hasName) {
    [output writeString:2 value:self.name];
  }
  if (self.hasUrl) {
    [output writeString:3 value:self.url];
  }
  if (self.hasSpaceLimit) {
    [output writeInt64:4 value:self.spaceLimit];
  }
  if (self.hasUsedForWriting) {
    [output writeBool:5 value:self.usedForWriting];
  }
  [self.unknownFields writeToCodedOutputStream:output];
}
- (int32_t) serializedSize {
  int32_t size = memoizedSerializedSize;
  if (size != -1) {
    return size;
  }

  size = 0;
  if (self.hasId) {
    size += computeInt32Size(1, self.id);
  }
  if (self.hasName) {
    size += computeStringSize(2, self.name);
  }
  if (self.hasUrl) {
    size += computeStringSize(3, self.url);
  }
  if (self.hasSpaceLimit) {
    size += computeInt64Size(4, self.spaceLimit);
  }
  if (self.hasUsedForWriting) {
    size += computeBoolSize(5, self.usedForWriting);
  }
  size += self.unknownFields.serializedSize;
  memoizedSerializedSize = size;
  return size;
}
+ (Server_Storage*) parseFromData:(NSData*) data {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromData:data] build];
}
+ (Server_Storage*) parseFromData:(NSData*) data extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromData:data extensionRegistry:extensionRegistry] build];
}
+ (Server_Storage*) parseFromInputStream:(NSInputStream*) input {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromInputStream:input] build];
}
+ (Server_Storage*) parseFromInputStream:(NSInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Server_Storage*) parseFromCodedInputStream:(PBCodedInputStream*) input {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromCodedInputStream:input] build];
}
+ (Server_Storage*) parseFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Server_Storage*)[[[Server_Storage builder] mergeFromCodedInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Server_Storage_Builder*) builder {
  return [[Server_Storage_Builder alloc] init];
}
+ (Server_Storage_Builder*) builderWithPrototype:(Server_Storage*) prototype {
  return [[Server_Storage builder] mergeFrom:prototype];
}
- (Server_Storage_Builder*) builder {
  return [Server_Storage builder];
}
- (Server_Storage_Builder*) toBuilder {
  return [Server_Storage builderWithPrototype:self];
}
- (void) writeDescriptionTo:(NSMutableString*) output withIndent:(NSString*) indent {
  NSUInteger listCount = 0;
  if (self.hasId) {
    [output appendFormat:@"%@%@: %@\n", indent, @"id", [NSNumber numberWithInt:self.id]];
  }
  if (self.hasName) {
    [output appendFormat:@"%@%@: %@\n", indent, @"name", self.name];
  }
  if (self.hasUrl) {
    [output appendFormat:@"%@%@: %@\n", indent, @"url", self.url];
  }
  if (self.hasSpaceLimit) {
    [output appendFormat:@"%@%@: %@\n", indent, @"spaceLimit", [NSNumber numberWithLongLong:self.spaceLimit]];
  }
  if (self.hasUsedForWriting) {
    [output appendFormat:@"%@%@: %@\n", indent, @"usedForWriting", [NSNumber numberWithBool:self.usedForWriting]];
  }
  [self.unknownFields writeDescriptionTo:output withIndent:indent];
}
- (BOOL) isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (![other isKindOfClass:[Server_Storage class]]) {
    return NO;
  }
  Server_Storage *otherMessage = other;
  return
      self.hasId == otherMessage.hasId &&
      (!self.hasId || self.id == otherMessage.id) &&
      self.hasName == otherMessage.hasName &&
      (!self.hasName || [self.name isEqual:otherMessage.name]) &&
      self.hasUrl == otherMessage.hasUrl &&
      (!self.hasUrl || [self.url isEqual:otherMessage.url]) &&
      self.hasSpaceLimit == otherMessage.hasSpaceLimit &&
      (!self.hasSpaceLimit || self.spaceLimit == otherMessage.spaceLimit) &&
      self.hasUsedForWriting == otherMessage.hasUsedForWriting &&
      (!self.hasUsedForWriting || self.usedForWriting == otherMessage.usedForWriting) &&
      (self.unknownFields == otherMessage.unknownFields || (self.unknownFields != nil && [self.unknownFields isEqual:otherMessage.unknownFields]));
}
- (NSUInteger) hash {
  NSUInteger hashCode = 7;
  NSUInteger listCount = 0;
  if (self.hasId) {
    hashCode = hashCode * 31 + [[NSNumber numberWithInt:self.id] hash];
  }
  if (self.hasName) {
    hashCode = hashCode * 31 + [self.name hash];
  }
  if (self.hasUrl) {
    hashCode = hashCode * 31 + [self.url hash];
  }
  if (self.hasSpaceLimit) {
    hashCode = hashCode * 31 + [[NSNumber numberWithLongLong:self.spaceLimit] hash];
  }
  if (self.hasUsedForWriting) {
    hashCode = hashCode * 31 + [[NSNumber numberWithBool:self.usedForWriting] hash];
  }
  hashCode = hashCode * 31 + [self.unknownFields hash];
  return hashCode;
}
@end

@interface Server_Storage_Builder()
@property (strong) Server_Storage* result;
@end

@implementation Server_Storage_Builder
@synthesize result;
- (id) init {
  if ((self = [super init])) {
    self.result = [[Server_Storage alloc] init];
  }
  return self;
}
- (PBGeneratedMessage*) internalGetResult {
  return result;
}
- (Server_Storage_Builder*) clear {
  self.result = [[Server_Storage alloc] init];
  return self;
}
- (Server_Storage_Builder*) clone {
  return [Server_Storage builderWithPrototype:result];
}
- (Server_Storage*) defaultInstance {
  return [Server_Storage defaultInstance];
}
- (Server_Storage*) build {
  [self checkInitialized];
  return [self buildPartial];
}
- (Server_Storage*) buildPartial {
  Server_Storage* returnMe = result;
  self.result = nil;
  return returnMe;
}
- (Server_Storage_Builder*) mergeFrom:(Server_Storage*) other {
  if (other == [Server_Storage defaultInstance]) {
    return self;
  }
  if (other.hasId) {
    [self setId:other.id];
  }
  if (other.hasName) {
    [self setName:other.name];
  }
  if (other.hasUrl) {
    [self setUrl:other.url];
  }
  if (other.hasSpaceLimit) {
    [self setSpaceLimit:other.spaceLimit];
  }
  if (other.hasUsedForWriting) {
    [self setUsedForWriting:other.usedForWriting];
  }
  [self mergeUnknownFields:other.unknownFields];
  return self;
}
- (Server_Storage_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input {
  return [self mergeFromCodedInputStream:input extensionRegistry:[PBExtensionRegistry emptyRegistry]];
}
- (Server_Storage_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  PBUnknownFieldSet_Builder* unknownFields = [PBUnknownFieldSet builderWithUnknownFields:self.unknownFields];
  while (YES) {
    int32_t tag = [input readTag];
    switch (tag) {
      case 0:
        [self setUnknownFields:[unknownFields build]];
        return self;
      default: {
        if (![self parseUnknownField:input unknownFields:unknownFields extensionRegistry:extensionRegistry tag:tag]) {
          [self setUnknownFields:[unknownFields build]];
          return self;
        }
        break;
      }
      case 8: {
        [self setId:[input readInt32]];
        break;
      }
      case 18: {
        [self setName:[input readString]];
        break;
      }
      case 26: {
        [self setUrl:[input readString]];
        break;
      }
      case 32: {
        [self setSpaceLimit:[input readInt64]];
        break;
      }
      case 40: {
        [self setUsedForWriting:[input readBool]];
        break;
      }
    }
  }
}
- (BOOL) hasId {
  return result.hasId;
}
- (int32_t) id {
  return result.id;
}
- (Server_Storage_Builder*) setId:(int32_t) value {
  result.hasId = YES;
  result.id = value;
  return self;
}
- (Server_Storage_Builder*) clearId {
  result.hasId = NO;
  result.id = 0;
  return self;
}
- (BOOL) hasName {
  return result.hasName;
}
- (NSString*) name {
  return result.name;
}
- (Server_Storage_Builder*) setName:(NSString*) value {
  result.hasName = YES;
  result.name = value;
  return self;
}
- (Server_Storage_Builder*) clearName {
  result.hasName = NO;
  result.name = @"";
  return self;
}
- (BOOL) hasUrl {
  return result.hasUrl;
}
- (NSString*) url {
  return result.url;
}
- (Server_Storage_Builder*) setUrl:(NSString*) value {
  result.hasUrl = YES;
  result.url = value;
  return self;
}
- (Server_Storage_Builder*) clearUrl {
  result.hasUrl = NO;
  result.url = @"";
  return self;
}
- (BOOL) hasSpaceLimit {
  return result.hasSpaceLimit;
}
- (int64_t) spaceLimit {
  return result.spaceLimit;
}
- (Server_Storage_Builder*) setSpaceLimit:(int64_t) value {
  result.hasSpaceLimit = YES;
  result.spaceLimit = value;
  return self;
}
- (Server_Storage_Builder*) clearSpaceLimit {
  result.hasSpaceLimit = NO;
  result.spaceLimit = 0L;
  return self;
}
- (BOOL) hasUsedForWriting {
  return result.hasUsedForWriting;
}
- (BOOL) usedForWriting {
  return result.usedForWriting;
}
- (Server_Storage_Builder*) setUsedForWriting:(BOOL) value {
  result.hasUsedForWriting = YES;
  result.usedForWriting = value;
  return self;
}
- (Server_Storage_Builder*) clearUsedForWriting {
  result.hasUsedForWriting = NO;
  result.usedForWriting = NO;
  return self;
}
@end

@interface Server_Builder()
@property (strong) Server* result;
@end

@implementation Server_Builder
@synthesize result;
- (id) init {
  if ((self = [super init])) {
    self.result = [[Server alloc] init];
  }
  return self;
}
- (PBGeneratedMessage*) internalGetResult {
  return result;
}
- (Server_Builder*) clear {
  self.result = [[Server alloc] init];
  return self;
}
- (Server_Builder*) clone {
  return [Server builderWithPrototype:result];
}
- (Server*) defaultInstance {
  return [Server defaultInstance];
}
- (Server*) build {
  [self checkInitialized];
  return [self buildPartial];
}
- (Server*) buildPartial {
  Server* returnMe = result;
  self.result = nil;
  return returnMe;
}
- (Server_Builder*) mergeFrom:(Server*) other {
  if (other == [Server defaultInstance]) {
    return self;
  }
  if (other.hasApiUrl) {
    [self setApiUrl:other.apiUrl];
  }
  if (other.hasNetAddrList) {
    [self setNetAddrList:other.netAddrList];
  }
  if (other.hasReserve) {
    [self setReserve:other.reserve];
  }
  if (other.storageArray.count > 0) {
    if (result.storageArray == nil) {
      result.storageArray = [[NSMutableArray alloc] initWithArray:other.storageArray];
    } else {
      [result.storageArray addObjectsFromArray:other.storageArray];
    }
  }
  if (other.hasPanicMode) {
    [self setPanicMode:other.panicMode];
  }
  if (other.hasStreamingUrl) {
    [self setStreamingUrl:other.streamingUrl];
  }
  if (other.hasVersion) {
    [self setVersion:other.version];
  }
  [self mergeUnknownFields:other.unknownFields];
  return self;
}
- (Server_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input {
  return [self mergeFromCodedInputStream:input extensionRegistry:[PBExtensionRegistry emptyRegistry]];
}
- (Server_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  PBUnknownFieldSet_Builder* unknownFields = [PBUnknownFieldSet builderWithUnknownFields:self.unknownFields];
  while (YES) {
    int32_t tag = [input readTag];
    switch (tag) {
      case 0:
        [self setUnknownFields:[unknownFields build]];
        return self;
      default: {
        if (![self parseUnknownField:input unknownFields:unknownFields extensionRegistry:extensionRegistry tag:tag]) {
          [self setUnknownFields:[unknownFields build]];
          return self;
        }
        break;
      }
      case 10: {
        [self setApiUrl:[input readString]];
        break;
      }
      case 18: {
        [self setNetAddrList:[input readString]];
        break;
      }
      case 24: {
        [self setReserve:[input readBool]];
        break;
      }
      case 34: {
        Server_Storage_Builder* subBuilder = [Server_Storage builder];
        [input readMessage:subBuilder extensionRegistry:extensionRegistry];
        [self addStorage:[subBuilder buildPartial]];
        break;
      }
      case 40: {
        [self setPanicMode:[input readInt32]];
        break;
      }
      case 50: {
        [self setStreamingUrl:[input readString]];
        break;
      }
      case 58: {
        [self setVersion:[input readString]];
        break;
      }
    }
  }
}
- (BOOL) hasApiUrl {
  return result.hasApiUrl;
}
- (NSString*) apiUrl {
  return result.apiUrl;
}
- (Server_Builder*) setApiUrl:(NSString*) value {
  result.hasApiUrl = YES;
  result.apiUrl = value;
  return self;
}
- (Server_Builder*) clearApiUrl {
  result.hasApiUrl = NO;
  result.apiUrl = @"";
  return self;
}
- (BOOL) hasNetAddrList {
  return result.hasNetAddrList;
}
- (NSString*) netAddrList {
  return result.netAddrList;
}
- (Server_Builder*) setNetAddrList:(NSString*) value {
  result.hasNetAddrList = YES;
  result.netAddrList = value;
  return self;
}
- (Server_Builder*) clearNetAddrList {
  result.hasNetAddrList = NO;
  result.netAddrList = @"";
  return self;
}
- (BOOL) hasReserve {
  return result.hasReserve;
}
- (BOOL) reserve {
  return result.reserve;
}
- (Server_Builder*) setReserve:(BOOL) value {
  result.hasReserve = YES;
  result.reserve = value;
  return self;
}
- (Server_Builder*) clearReserve {
  result.hasReserve = NO;
  result.reserve = NO;
  return self;
}
- (NSMutableArray *)storage {
  return result.storageArray;
}
- (Server_Storage*)storageAtIndex:(NSUInteger)index {
  return [result storageAtIndex:index];
}
- (Server_Builder *)addStorage:(Server_Storage*)value {
  if (result.storageArray == nil) {
    result.storageArray = [[NSMutableArray alloc]init];
  }
  [result.storageArray addObject:value];
  return self;
}
- (Server_Builder *)setStorageArray:(NSArray *)array {
  result.storageArray = [[NSMutableArray alloc]init];
  return self;
}
- (Server_Builder *)clearStorage {
  result.storageArray = nil;
  return self;
}
- (BOOL) hasPanicMode {
  return result.hasPanicMode;
}
- (int32_t) panicMode {
  return result.panicMode;
}
- (Server_Builder*) setPanicMode:(int32_t) value {
  result.hasPanicMode = YES;
  result.panicMode = value;
  return self;
}
- (Server_Builder*) clearPanicMode {
  result.hasPanicMode = NO;
  result.panicMode = 0;
  return self;
}
- (BOOL) hasStreamingUrl {
  return result.hasStreamingUrl;
}
- (NSString*) streamingUrl {
  return result.streamingUrl;
}
- (Server_Builder*) setStreamingUrl:(NSString*) value {
  result.hasStreamingUrl = YES;
  result.streamingUrl = value;
  return self;
}
- (Server_Builder*) clearStreamingUrl {
  result.hasStreamingUrl = NO;
  result.streamingUrl = @"";
  return self;
}
- (BOOL) hasVersion {
  return result.hasVersion;
}
- (NSString*) version {
  return result.version;
}
- (Server_Builder*) setVersion:(NSString*) value {
  result.hasVersion = YES;
  result.version = value;
  return self;
}
- (Server_Builder*) clearVersion {
  result.hasVersion = NO;
  result.version = @"";
  return self;
}
@end

