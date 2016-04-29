#import <Foundation/Foundation.h>

@interface HDWOrderedDictionary : NSObject {
	NSMutableDictionary *_dictionary;
	NSMutableArray *_array;
}

+ (instancetype)dictionary;

- (NSUInteger)count;
- (void)setObject:(id)anObject forKey:(id)aKey atIndex:(NSUInteger)anIndex;
- (void)removeObjectForKey:(id)key;
- (void)removeAllObjects;

- (id)keyAtIndex:(NSUInteger)anIndex;
- (id)valueAtIndex:(NSUInteger)anIndex;
- (NSUInteger)indexOfKey:(id)key;
- (id)objectForKey:(id)key;

// Support keyed subscripting
- (id)objectForKeyedSubscript:(id <NSCopying>)key;
- (void)setObject:(id)obj forKeyedSubscript:(id <NSCopying>)key;

- (NSEnumerator *)valuesEnumerator;
- (NSEnumerator *)reverseKeyEnumerator;

@end
