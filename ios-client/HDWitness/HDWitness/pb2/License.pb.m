// Generated by the protocol buffer compiler.  DO NOT EDIT!

#import "License.pb.h"

@implementation LicenseRoot
static PBExtensionRegistry* extensionRegistry = nil;
+ (PBExtensionRegistry*) extensionRegistry {
  return extensionRegistry;
}

+ (void) initialize {
  if (self == [LicenseRoot class]) {
    PBMutableExtensionRegistry* registry = [PBMutableExtensionRegistry registry];
    [self registerAllExtensions:registry];
    extensionRegistry = registry;
  }
}
+ (void) registerAllExtensions:(PBMutableExtensionRegistry*) registry {
}
@end

@interface License ()
@property (strong) NSString* name;
@property (strong) NSString* key;
@property int32_t cameraCount;
@property (strong) NSString* hwid;
@property (strong) NSString* signature;
@property (strong) NSString* rawLicense;
@end

@implementation License

- (BOOL) hasName {
  return !!hasName_;
}
- (void) setHasName:(BOOL) value {
  hasName_ = !!value;
}
@synthesize name;
- (BOOL) hasKey {
  return !!hasKey_;
}
- (void) setHasKey:(BOOL) value {
  hasKey_ = !!value;
}
@synthesize key;
- (BOOL) hasCameraCount {
  return !!hasCameraCount_;
}
- (void) setHasCameraCount:(BOOL) value {
  hasCameraCount_ = !!value;
}
@synthesize cameraCount;
- (BOOL) hasHwid {
  return !!hasHwid_;
}
- (void) setHasHwid:(BOOL) value {
  hasHwid_ = !!value;
}
@synthesize hwid;
- (BOOL) hasSignature {
  return !!hasSignature_;
}
- (void) setHasSignature:(BOOL) value {
  hasSignature_ = !!value;
}
@synthesize signature;
- (BOOL) hasRawLicense {
  return !!hasRawLicense_;
}
- (void) setHasRawLicense:(BOOL) value {
  hasRawLicense_ = !!value;
}
@synthesize rawLicense;
- (id) init {
  if ((self = [super init])) {
    self.name = @"";
    self.key = @"";
    self.cameraCount = 0;
    self.hwid = @"";
    self.signature = @"";
    self.rawLicense = @"";
  }
  return self;
}
static License* defaultLicenseInstance = nil;
+ (void) initialize {
  if (self == [License class]) {
    defaultLicenseInstance = [[License alloc] init];
  }
}
+ (License*) defaultInstance {
  return defaultLicenseInstance;
}
- (License*) defaultInstance {
  return defaultLicenseInstance;
}
- (BOOL) isInitialized {
  if (!self.hasName) {
    return NO;
  }
  if (!self.hasKey) {
    return NO;
  }
  if (!self.hasCameraCount) {
    return NO;
  }
  if (!self.hasHwid) {
    return NO;
  }
  if (!self.hasSignature) {
    return NO;
  }
  return YES;
}
- (void) writeToCodedOutputStream:(PBCodedOutputStream*) output {
  if (self.hasName) {
    [output writeString:1 value:self.name];
  }
  if (self.hasKey) {
    [output writeString:2 value:self.key];
  }
  if (self.hasCameraCount) {
    [output writeInt32:3 value:self.cameraCount];
  }
  if (self.hasHwid) {
    [output writeString:4 value:self.hwid];
  }
  if (self.hasSignature) {
    [output writeString:5 value:self.signature];
  }
  if (self.hasRawLicense) {
    [output writeString:6 value:self.rawLicense];
  }
  [self.unknownFields writeToCodedOutputStream:output];
}
- (int32_t) serializedSize {
  int32_t size = memoizedSerializedSize;
  if (size != -1) {
    return size;
  }

  size = 0;
  if (self.hasName) {
    size += computeStringSize(1, self.name);
  }
  if (self.hasKey) {
    size += computeStringSize(2, self.key);
  }
  if (self.hasCameraCount) {
    size += computeInt32Size(3, self.cameraCount);
  }
  if (self.hasHwid) {
    size += computeStringSize(4, self.hwid);
  }
  if (self.hasSignature) {
    size += computeStringSize(5, self.signature);
  }
  if (self.hasRawLicense) {
    size += computeStringSize(6, self.rawLicense);
  }
  size += self.unknownFields.serializedSize;
  memoizedSerializedSize = size;
  return size;
}
+ (License*) parseFromData:(NSData*) data {
  return (License*)[[[License builder] mergeFromData:data] build];
}
+ (License*) parseFromData:(NSData*) data extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (License*)[[[License builder] mergeFromData:data extensionRegistry:extensionRegistry] build];
}
+ (License*) parseFromInputStream:(NSInputStream*) input {
  return (License*)[[[License builder] mergeFromInputStream:input] build];
}
+ (License*) parseFromInputStream:(NSInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (License*)[[[License builder] mergeFromInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (License*) parseFromCodedInputStream:(PBCodedInputStream*) input {
  return (License*)[[[License builder] mergeFromCodedInputStream:input] build];
}
+ (License*) parseFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (License*)[[[License builder] mergeFromCodedInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (License_Builder*) builder {
  return [[License_Builder alloc] init];
}
+ (License_Builder*) builderWithPrototype:(License*) prototype {
  return [[License builder] mergeFrom:prototype];
}
- (License_Builder*) builder {
  return [License builder];
}
- (License_Builder*) toBuilder {
  return [License builderWithPrototype:self];
}
- (void) writeDescriptionTo:(NSMutableString*) output withIndent:(NSString*) indent {
  NSUInteger listCount = 0;
  if (self.hasName) {
    [output appendFormat:@"%@%@: %@\n", indent, @"name", self.name];
  }
  if (self.hasKey) {
    [output appendFormat:@"%@%@: %@\n", indent, @"key", self.key];
  }
  if (self.hasCameraCount) {
    [output appendFormat:@"%@%@: %@\n", indent, @"cameraCount", [NSNumber numberWithInt:self.cameraCount]];
  }
  if (self.hasHwid) {
    [output appendFormat:@"%@%@: %@\n", indent, @"hwid", self.hwid];
  }
  if (self.hasSignature) {
    [output appendFormat:@"%@%@: %@\n", indent, @"signature", self.signature];
  }
  if (self.hasRawLicense) {
    [output appendFormat:@"%@%@: %@\n", indent, @"rawLicense", self.rawLicense];
  }
  [self.unknownFields writeDescriptionTo:output withIndent:indent];
}
- (BOOL) isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (![other isKindOfClass:[License class]]) {
    return NO;
  }
  License *otherMessage = other;
  return
      self.hasName == otherMessage.hasName &&
      (!self.hasName || [self.name isEqual:otherMessage.name]) &&
      self.hasKey == otherMessage.hasKey &&
      (!self.hasKey || [self.key isEqual:otherMessage.key]) &&
      self.hasCameraCount == otherMessage.hasCameraCount &&
      (!self.hasCameraCount || self.cameraCount == otherMessage.cameraCount) &&
      self.hasHwid == otherMessage.hasHwid &&
      (!self.hasHwid || [self.hwid isEqual:otherMessage.hwid]) &&
      self.hasSignature == otherMessage.hasSignature &&
      (!self.hasSignature || [self.signature isEqual:otherMessage.signature]) &&
      self.hasRawLicense == otherMessage.hasRawLicense &&
      (!self.hasRawLicense || [self.rawLicense isEqual:otherMessage.rawLicense]) &&
      (self.unknownFields == otherMessage.unknownFields || (self.unknownFields != nil && [self.unknownFields isEqual:otherMessage.unknownFields]));
}
- (NSUInteger) hash {
  NSUInteger hashCode = 7;
  NSUInteger listCount = 0;
  if (self.hasName) {
    hashCode = hashCode * 31 + [self.name hash];
  }
  if (self.hasKey) {
    hashCode = hashCode * 31 + [self.key hash];
  }
  if (self.hasCameraCount) {
    hashCode = hashCode * 31 + [[NSNumber numberWithInt:self.cameraCount] hash];
  }
  if (self.hasHwid) {
    hashCode = hashCode * 31 + [self.hwid hash];
  }
  if (self.hasSignature) {
    hashCode = hashCode * 31 + [self.signature hash];
  }
  if (self.hasRawLicense) {
    hashCode = hashCode * 31 + [self.rawLicense hash];
  }
  hashCode = hashCode * 31 + [self.unknownFields hash];
  return hashCode;
}
@end

@interface License_Builder()
@property (strong) License* result;
@end

@implementation License_Builder
@synthesize result;
- (id) init {
  if ((self = [super init])) {
    self.result = [[License alloc] init];
  }
  return self;
}
- (PBGeneratedMessage*) internalGetResult {
  return result;
}
- (License_Builder*) clear {
  self.result = [[License alloc] init];
  return self;
}
- (License_Builder*) clone {
  return [License builderWithPrototype:result];
}
- (License*) defaultInstance {
  return [License defaultInstance];
}
- (License*) build {
  [self checkInitialized];
  return [self buildPartial];
}
- (License*) buildPartial {
  License* returnMe = result;
  self.result = nil;
  return returnMe;
}
- (License_Builder*) mergeFrom:(License*) other {
  if (other == [License defaultInstance]) {
    return self;
  }
  if (other.hasName) {
    [self setName:other.name];
  }
  if (other.hasKey) {
    [self setKey:other.key];
  }
  if (other.hasCameraCount) {
    [self setCameraCount:other.cameraCount];
  }
  if (other.hasHwid) {
    [self setHwid:other.hwid];
  }
  if (other.hasSignature) {
    [self setSignature:other.signature];
  }
  if (other.hasRawLicense) {
    [self setRawLicense:other.rawLicense];
  }
  [self mergeUnknownFields:other.unknownFields];
  return self;
}
- (License_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input {
  return [self mergeFromCodedInputStream:input extensionRegistry:[PBExtensionRegistry emptyRegistry]];
}
- (License_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
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
        [self setName:[input readString]];
        break;
      }
      case 18: {
        [self setKey:[input readString]];
        break;
      }
      case 24: {
        [self setCameraCount:[input readInt32]];
        break;
      }
      case 34: {
        [self setHwid:[input readString]];
        break;
      }
      case 42: {
        [self setSignature:[input readString]];
        break;
      }
      case 50: {
        [self setRawLicense:[input readString]];
        break;
      }
    }
  }
}
- (BOOL) hasName {
  return result.hasName;
}
- (NSString*) name {
  return result.name;
}
- (License_Builder*) setName:(NSString*) value {
  result.hasName = YES;
  result.name = value;
  return self;
}
- (License_Builder*) clearName {
  result.hasName = NO;
  result.name = @"";
  return self;
}
- (BOOL) hasKey {
  return result.hasKey;
}
- (NSString*) key {
  return result.key;
}
- (License_Builder*) setKey:(NSString*) value {
  result.hasKey = YES;
  result.key = value;
  return self;
}
- (License_Builder*) clearKey {
  result.hasKey = NO;
  result.key = @"";
  return self;
}
- (BOOL) hasCameraCount {
  return result.hasCameraCount;
}
- (int32_t) cameraCount {
  return result.cameraCount;
}
- (License_Builder*) setCameraCount:(int32_t) value {
  result.hasCameraCount = YES;
  result.cameraCount = value;
  return self;
}
- (License_Builder*) clearCameraCount {
  result.hasCameraCount = NO;
  result.cameraCount = 0;
  return self;
}
- (BOOL) hasHwid {
  return result.hasHwid;
}
- (NSString*) hwid {
  return result.hwid;
}
- (License_Builder*) setHwid:(NSString*) value {
  result.hasHwid = YES;
  result.hwid = value;
  return self;
}
- (License_Builder*) clearHwid {
  result.hasHwid = NO;
  result.hwid = @"";
  return self;
}
- (BOOL) hasSignature {
  return result.hasSignature;
}
- (NSString*) signature {
  return result.signature;
}
- (License_Builder*) setSignature:(NSString*) value {
  result.hasSignature = YES;
  result.signature = value;
  return self;
}
- (License_Builder*) clearSignature {
  result.hasSignature = NO;
  result.signature = @"";
  return self;
}
- (BOOL) hasRawLicense {
  return result.hasRawLicense;
}
- (NSString*) rawLicense {
  return result.rawLicense;
}
- (License_Builder*) setRawLicense:(NSString*) value {
  result.hasRawLicense = YES;
  result.rawLicense = value;
  return self;
}
- (License_Builder*) clearRawLicense {
  result.hasRawLicense = NO;
  result.rawLicense = @"";
  return self;
}
@end

@interface Licenses ()
@property (strong) NSMutableArray * licenseArray;
@property (strong) NSString* unusedHwid1;
@property (strong) NSString* unusedOldHardwareId;
@property (strong) NSString* unusedHwid2;
@end

@implementation Licenses

@synthesize licenseArray;
@dynamic license;
- (BOOL) hasUnusedHwid1 {
  return !!hasUnusedHwid1_;
}
- (void) setHasUnusedHwid1:(BOOL) value {
  hasUnusedHwid1_ = !!value;
}
@synthesize unusedHwid1;
- (BOOL) hasUnusedOldHardwareId {
  return !!hasUnusedOldHardwareId_;
}
- (void) setHasUnusedOldHardwareId:(BOOL) value {
  hasUnusedOldHardwareId_ = !!value;
}
@synthesize unusedOldHardwareId;
- (BOOL) hasUnusedHwid2 {
  return !!hasUnusedHwid2_;
}
- (void) setHasUnusedHwid2:(BOOL) value {
  hasUnusedHwid2_ = !!value;
}
@synthesize unusedHwid2;
- (id) init {
  if ((self = [super init])) {
    self.unusedHwid1 = @"";
    self.unusedOldHardwareId = @"";
    self.unusedHwid2 = @"";
  }
  return self;
}
static Licenses* defaultLicensesInstance = nil;
+ (void) initialize {
  if (self == [Licenses class]) {
    defaultLicensesInstance = [[Licenses alloc] init];
  }
}
+ (Licenses*) defaultInstance {
  return defaultLicensesInstance;
}
- (Licenses*) defaultInstance {
  return defaultLicensesInstance;
}
- (NSArray *)license {
  return licenseArray;
}
- (License*)licenseAtIndex:(NSUInteger)index {
  return [licenseArray objectAtIndex:index];
}
- (BOOL) isInitialized {
  for (License* element in self.license) {
    if (!element.isInitialized) {
      return NO;
    }
  }
  return YES;
}
- (void) writeToCodedOutputStream:(PBCodedOutputStream*) output {
  for (License *element in self.licenseArray) {
    [output writeMessage:1 value:element];
  }
  if (self.hasUnusedHwid1) {
    [output writeString:2 value:self.unusedHwid1];
  }
  if (self.hasUnusedOldHardwareId) {
    [output writeString:3 value:self.unusedOldHardwareId];
  }
  if (self.hasUnusedHwid2) {
    [output writeString:4 value:self.unusedHwid2];
  }
  [self.unknownFields writeToCodedOutputStream:output];
}
- (int32_t) serializedSize {
  int32_t size = memoizedSerializedSize;
  if (size != -1) {
    return size;
  }

  size = 0;
  for (License *element in self.licenseArray) {
    size += computeMessageSize(1, element);
  }
  if (self.hasUnusedHwid1) {
    size += computeStringSize(2, self.unusedHwid1);
  }
  if (self.hasUnusedOldHardwareId) {
    size += computeStringSize(3, self.unusedOldHardwareId);
  }
  if (self.hasUnusedHwid2) {
    size += computeStringSize(4, self.unusedHwid2);
  }
  size += self.unknownFields.serializedSize;
  memoizedSerializedSize = size;
  return size;
}
+ (Licenses*) parseFromData:(NSData*) data {
  return (Licenses*)[[[Licenses builder] mergeFromData:data] build];
}
+ (Licenses*) parseFromData:(NSData*) data extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Licenses*)[[[Licenses builder] mergeFromData:data extensionRegistry:extensionRegistry] build];
}
+ (Licenses*) parseFromInputStream:(NSInputStream*) input {
  return (Licenses*)[[[Licenses builder] mergeFromInputStream:input] build];
}
+ (Licenses*) parseFromInputStream:(NSInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Licenses*)[[[Licenses builder] mergeFromInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Licenses*) parseFromCodedInputStream:(PBCodedInputStream*) input {
  return (Licenses*)[[[Licenses builder] mergeFromCodedInputStream:input] build];
}
+ (Licenses*) parseFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
  return (Licenses*)[[[Licenses builder] mergeFromCodedInputStream:input extensionRegistry:extensionRegistry] build];
}
+ (Licenses_Builder*) builder {
  return [[Licenses_Builder alloc] init];
}
+ (Licenses_Builder*) builderWithPrototype:(Licenses*) prototype {
  return [[Licenses builder] mergeFrom:prototype];
}
- (Licenses_Builder*) builder {
  return [Licenses builder];
}
- (Licenses_Builder*) toBuilder {
  return [Licenses builderWithPrototype:self];
}
- (void) writeDescriptionTo:(NSMutableString*) output withIndent:(NSString*) indent {
  NSUInteger listCount = 0;
  for (License* element in self.licenseArray) {
    [output appendFormat:@"%@%@ {\n", indent, @"license"];
    [element writeDescriptionTo:output
                     withIndent:[NSString stringWithFormat:@"%@  ", indent]];
    [output appendFormat:@"%@}\n", indent];
  }
  if (self.hasUnusedHwid1) {
    [output appendFormat:@"%@%@: %@\n", indent, @"unusedHwid1", self.unusedHwid1];
  }
  if (self.hasUnusedOldHardwareId) {
    [output appendFormat:@"%@%@: %@\n", indent, @"unusedOldHardwareId", self.unusedOldHardwareId];
  }
  if (self.hasUnusedHwid2) {
    [output appendFormat:@"%@%@: %@\n", indent, @"unusedHwid2", self.unusedHwid2];
  }
  [self.unknownFields writeDescriptionTo:output withIndent:indent];
}
- (BOOL) isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (![other isKindOfClass:[Licenses class]]) {
    return NO;
  }
  Licenses *otherMessage = other;
  return
      [self.licenseArray isEqualToArray:otherMessage.licenseArray] &&
      self.hasUnusedHwid1 == otherMessage.hasUnusedHwid1 &&
      (!self.hasUnusedHwid1 || [self.unusedHwid1 isEqual:otherMessage.unusedHwid1]) &&
      self.hasUnusedOldHardwareId == otherMessage.hasUnusedOldHardwareId &&
      (!self.hasUnusedOldHardwareId || [self.unusedOldHardwareId isEqual:otherMessage.unusedOldHardwareId]) &&
      self.hasUnusedHwid2 == otherMessage.hasUnusedHwid2 &&
      (!self.hasUnusedHwid2 || [self.unusedHwid2 isEqual:otherMessage.unusedHwid2]) &&
      (self.unknownFields == otherMessage.unknownFields || (self.unknownFields != nil && [self.unknownFields isEqual:otherMessage.unknownFields]));
}
- (NSUInteger) hash {
  NSUInteger hashCode = 7;
  NSUInteger listCount = 0;
  for (License* element in self.licenseArray) {
    hashCode = hashCode * 31 + [element hash];
  }
  if (self.hasUnusedHwid1) {
    hashCode = hashCode * 31 + [self.unusedHwid1 hash];
  }
  if (self.hasUnusedOldHardwareId) {
    hashCode = hashCode * 31 + [self.unusedOldHardwareId hash];
  }
  if (self.hasUnusedHwid2) {
    hashCode = hashCode * 31 + [self.unusedHwid2 hash];
  }
  hashCode = hashCode * 31 + [self.unknownFields hash];
  return hashCode;
}
@end

@interface Licenses_Builder()
@property (strong) Licenses* result;
@end

@implementation Licenses_Builder
@synthesize result;
- (id) init {
  if ((self = [super init])) {
    self.result = [[Licenses alloc] init];
  }
  return self;
}
- (PBGeneratedMessage*) internalGetResult {
  return result;
}
- (Licenses_Builder*) clear {
  self.result = [[Licenses alloc] init];
  return self;
}
- (Licenses_Builder*) clone {
  return [Licenses builderWithPrototype:result];
}
- (Licenses*) defaultInstance {
  return [Licenses defaultInstance];
}
- (Licenses*) build {
  [self checkInitialized];
  return [self buildPartial];
}
- (Licenses*) buildPartial {
  Licenses* returnMe = result;
  self.result = nil;
  return returnMe;
}
- (Licenses_Builder*) mergeFrom:(Licenses*) other {
  if (other == [Licenses defaultInstance]) {
    return self;
  }
  if (other.licenseArray.count > 0) {
    if (result.licenseArray == nil) {
      result.licenseArray = [[NSMutableArray alloc] initWithArray:other.licenseArray];
    } else {
      [result.licenseArray addObjectsFromArray:other.licenseArray];
    }
  }
  if (other.hasUnusedHwid1) {
    [self setUnusedHwid1:other.unusedHwid1];
  }
  if (other.hasUnusedOldHardwareId) {
    [self setUnusedOldHardwareId:other.unusedOldHardwareId];
  }
  if (other.hasUnusedHwid2) {
    [self setUnusedHwid2:other.unusedHwid2];
  }
  [self mergeUnknownFields:other.unknownFields];
  return self;
}
- (Licenses_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input {
  return [self mergeFromCodedInputStream:input extensionRegistry:[PBExtensionRegistry emptyRegistry]];
}
- (Licenses_Builder*) mergeFromCodedInputStream:(PBCodedInputStream*) input extensionRegistry:(PBExtensionRegistry*) extensionRegistry {
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
        License_Builder* subBuilder = [License builder];
        [input readMessage:subBuilder extensionRegistry:extensionRegistry];
        [self addLicense:[subBuilder buildPartial]];
        break;
      }
      case 18: {
        [self setUnusedHwid1:[input readString]];
        break;
      }
      case 26: {
        [self setUnusedOldHardwareId:[input readString]];
        break;
      }
      case 34: {
        [self setUnusedHwid2:[input readString]];
        break;
      }
    }
  }
}
- (NSMutableArray *)license {
  return result.licenseArray;
}
- (License*)licenseAtIndex:(NSUInteger)index {
  return [result licenseAtIndex:index];
}
- (Licenses_Builder *)addLicense:(License*)value {
  if (result.licenseArray == nil) {
    result.licenseArray = [[NSMutableArray alloc]init];
  }
  [result.licenseArray addObject:value];
  return self;
}
- (Licenses_Builder *)setLicenseArray:(NSArray *)array {
  result.licenseArray = [[NSMutableArray alloc]init];
  return self;
}
- (Licenses_Builder *)clearLicense {
  result.licenseArray = nil;
  return self;
}
- (BOOL) hasUnusedHwid1 {
  return result.hasUnusedHwid1;
}
- (NSString*) unusedHwid1 {
  return result.unusedHwid1;
}
- (Licenses_Builder*) setUnusedHwid1:(NSString*) value {
  result.hasUnusedHwid1 = YES;
  result.unusedHwid1 = value;
  return self;
}
- (Licenses_Builder*) clearUnusedHwid1 {
  result.hasUnusedHwid1 = NO;
  result.unusedHwid1 = @"";
  return self;
}
- (BOOL) hasUnusedOldHardwareId {
  return result.hasUnusedOldHardwareId;
}
- (NSString*) unusedOldHardwareId {
  return result.unusedOldHardwareId;
}
- (Licenses_Builder*) setUnusedOldHardwareId:(NSString*) value {
  result.hasUnusedOldHardwareId = YES;
  result.unusedOldHardwareId = value;
  return self;
}
- (Licenses_Builder*) clearUnusedOldHardwareId {
  result.hasUnusedOldHardwareId = NO;
  result.unusedOldHardwareId = @"";
  return self;
}
- (BOOL) hasUnusedHwid2 {
  return result.hasUnusedHwid2;
}
- (NSString*) unusedHwid2 {
  return result.unusedHwid2;
}
- (Licenses_Builder*) setUnusedHwid2:(NSString*) value {
  result.hasUnusedHwid2 = YES;
  result.unusedHwid2 = value;
  return self;
}
- (Licenses_Builder*) clearUnusedHwid2 {
  result.hasUnusedHwid2 = NO;
  result.unusedHwid2 = @"";
  return self;
}
@end

