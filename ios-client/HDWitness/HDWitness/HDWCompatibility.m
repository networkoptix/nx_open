#include "HDWCompatibility.h"

NSString *stripVersion(NSString *version) {
    NSArray *versionList = [version componentsSeparatedByString:@"."];
    if (versionList.count < 2)
        return version;
    
    return [NSString stringWithFormat:@"%d.%d", [versionList[0] intValue], [versionList[1] intValue]];
}

NSArray *localCompatibilityItems();

@implementation HDWCompatibilityItem

- (BOOL)isEqual:(id)object {
    HDWCompatibilityItem *other = (HDWCompatibilityItem *)object;
    
    return [self.version isEqualToString:other.version] &&
            [self.component isEqualToString:other.component] &&
            [self.systemVersion isEqualToString:other.systemVersion];
}

+(HDWCompatibilityItem*) itemForVersion:(NSString*)version ofComponent:(NSString*)component andSystemVersion:(NSString*)systemVersion {
    HDWCompatibilityItem *item = [[HDWCompatibilityItem alloc] init];
    item.version = version;
    item.component = component;
    item.systemVersion = systemVersion;
    return item;
}

@end

@implementation HDWCompatibilityChecker {
    NSArray *_items;
}

- (HDWCompatibilityChecker*) init {
    return [super init];
}

- (HDWCompatibilityChecker*) initLocal {
    self = [self init];
    _items = localCompatibilityItems();
    return self;
}

- (HDWCompatibilityChecker*) initWithItems:(NSArray*)items {
    self = [self init];
    _items = items;
    return self;
}

- (NSUInteger)itemCount {
    return _items.count;
}

- (BOOL)isCompatibleComponent:(NSString*)comp1 ofVersion:(NSString*)ver1 withComponent:(NSString*)comp2 ofVersion:(NSString*)ver2 {
    NSString *ver1s = stripVersion(ver1);
    NSString *ver2s = stripVersion(ver2);
    
    if ([ver1s isEqualToString:ver2s])
        return true;
    
    return [_items containsObject:[HDWCompatibilityItem itemForVersion:ver1s ofComponent:comp1 andSystemVersion:ver2s]] ||
        [_items containsObject:[HDWCompatibilityItem itemForVersion:ver2s ofComponent:comp2 andSystemVersion:ver1s]];
}

@end