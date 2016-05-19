#import "HDWOrderedDictionary.h"

NSString *DescriptionForObject(NSObject *object, id locale, NSUInteger indent) {
	NSString *objectString;
	if ([object isKindOfClass:[NSString class]]) {
		objectString = (NSString *)object;
	}
	else if ([object respondsToSelector:@selector(descriptionWithLocale:indent:)]) {
		objectString = [(NSDictionary *)object descriptionWithLocale:locale indent:indent];
	}
	else if ([object respondsToSelector:@selector(descriptionWithLocale:)]) {
		objectString = [(NSSet *)object descriptionWithLocale:locale];
	}
	else {
		objectString = [object description];
	}
	return objectString;
}

@interface HDWOrderedDictionaryValuesEnumerator : NSEnumerator {
    HDWOrderedDictionary *_dictionary;
    NSUInteger _currentIndex;
}

- (id)initWithOrderedDictionary:(HDWOrderedDictionary*)dictionary;
@end

@implementation HDWOrderedDictionaryValuesEnumerator

- (id)initWithOrderedDictionary:(HDWOrderedDictionary*)aDictionary
{
    self = [super init];
    if (self)
    {
        _dictionary = aDictionary;
        _currentIndex = 0;
    }
    return self;
}

- (id)nextObject
{
    if (_currentIndex >= _dictionary.count)
        return nil;
    
    return [_dictionary valueAtIndex:_currentIndex++];
}

@end

@implementation HDWOrderedDictionary

+ (instancetype)dictionary {
    return [[HDWOrderedDictionary alloc] init];
}

- (instancetype)init {
    self = [super init];
    
    if (self) {
        _dictionary = [[NSMutableDictionary alloc] initWithCapacity:0];
        _array = [[NSMutableArray alloc] initWithCapacity:0];
    }
    
    return self;
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
	self = [super init];
    
	if (self != nil) {
		_dictionary = [[NSMutableDictionary alloc] initWithCapacity:capacity];
		_array = [[NSMutableArray alloc] initWithCapacity:capacity];
	}
	return self;
}

//- (id)copy {
//	return [self mutableCopy];
//}

- (void)setObject:(id)anObject forKey:(id)aKey {
	if (!_dictionary[aKey]) {
		[_array addObject:aKey];
	}
    
	_dictionary[aKey] = anObject;
}

- (NSUInteger)count {
	return [_dictionary count];
}

- (NSEnumerator *)keyEnumerator {
	return [_array objectEnumerator];
}

- (NSEnumerator *)valuesEnumerator {
    return [[HDWOrderedDictionaryValuesEnumerator alloc] initWithOrderedDictionary:self];
}
- (NSEnumerator *)reverseKeyEnumerator {
	return [_array reverseObjectEnumerator];
}

- (void)setObject:(id)anObject forKey:(id)aKey atIndex:(NSUInteger)anIndex {
	if (_dictionary[aKey]) {
		[self removeObjectForKey:aKey];
	}
    
	[_array insertObject:aKey atIndex:anIndex];
	_dictionary[aKey] = anObject;
}

- (void)removeObjectForKey:(id)key {
    [_dictionary removeObjectForKey:key];
    [_array removeObject:key];
}

- (void)removeAllObjects {
    [_dictionary removeAllObjects];
    [_array removeAllObjects];
}

- (id)keyAtIndex:(NSUInteger)anIndex {
	return _array[anIndex];
}

- (id)valueAtIndex:(NSUInteger)anIndex {
    id key = [self keyAtIndex:anIndex];
    return _dictionary[key];
}

- (NSUInteger)indexOfKey:(id)key {
    return [_array indexOfObject:key];
}

- (void)setObject:(id)obj forKeyedSubscript:(id <NSCopying>)key {
    [self setObject:obj forKey:key];
}

- (id)objectForKeyedSubscript:(id <NSCopying>)key {
    return [self objectForKey:key];
}

- (id)objectForKey:(id)key {
    return _dictionary[key];
}

- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
	NSMutableString *indentString = [NSMutableString string];
	NSUInteger i, count = level;
    
	for (i = 0; i < count; i++) {
		[indentString appendFormat:@"    "];
	}
	
	NSMutableString *description = [NSMutableString string];
	[description appendFormat:@"%@{\n", indentString];
    NSObject *key;
    while (key = [self.keyEnumerator nextObject]) {
        [description appendFormat:@"%@    %@ = %@;\n",
         indentString,
         DescriptionForObject(key, locale, level),
         DescriptionForObject([self objectForKey:key], locale, level)];
    }
	[description appendFormat:@"%@}\n", indentString];
	return description;
}

//- (id)copyWithZone:(NSZone *)zone {
//    HDWOrderedDictionary *newDictionary = [[HDWOrderedDictionary allocWithZone:zone] init];
//    
//    if (newDictionary) {
//        newDictionary->_dictionary = [_dictionary mutableCopyWithZone:zone];
//        newDictionary->_array = [_array mutableCopyWithZone:zone];
//    }
//    
//    return newDictionary;
//}
//
//- (id)mutableCopyWithZone:(NSZone *)zone {
//    HDWOrderedDictionary *newDictionary = [[HDWOrderedDictionary allocWithZone:zone] init];
//    
//    if (newDictionary) {
//        newDictionary->_dictionary = [_dictionary mutableCopyWithZone:zone];
//        newDictionary->_array = [_array mutableCopyWithZone:zone];
//    }
//    
//    return newDictionary;
//}

@end
