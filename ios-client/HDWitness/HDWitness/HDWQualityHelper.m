//
//  HDWQualityHelper.m
//  HDWitness
//
//  Created by Ivan Vigasin on 07/09/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWQualityHelper.h"

typedef enum {
    QS_NONE,
    QS_NAT1,
    QS_NAT2,
    QS_TRAN
} HDWQualityMode;

// r1, r2 -> quality selection array[4]
int quality_policy_table[][6] = {
    {0, 0, QS_TRAN, QS_TRAN, QS_TRAN, QS_TRAN},
    {1, 0, QS_NONE, QS_NONE, QS_NONE, QS_NAT1},
    {1, 1, QS_NONE, QS_NAT2, QS_NONE, QS_NAT1},
    {2, 0, QS_TRAN, QS_NONE, QS_NONE, QS_NAT1},
    {2, 1, QS_NONE, QS_NAT2, QS_NONE, QS_NAT1},
    {2, 2, QS_NONE, QS_NAT2, QS_NONE, QS_NAT1},
    {3, 0, QS_TRAN, QS_TRAN, QS_NONE, QS_NAT1},
    {3, 1, QS_NAT2, QS_TRAN, QS_NONE, QS_NAT1},
    {3, 2, QS_TRAN, QS_NAT2, QS_NONE, QS_NAT1},
    {3, 3, QS_TRAN, QS_TRAN, QS_NAT2, QS_NAT1},
    {4, 0, QS_TRAN, QS_TRAN, QS_TRAN, QS_NAT1},
    {4, 1, QS_NAT2, QS_TRAN, QS_TRAN, QS_NAT1},
    {4, 2, QS_TRAN, QS_NAT2, QS_TRAN, QS_NAT1},
    {4, 3, QS_TRAN, QS_TRAN, QS_NAT2, QS_NAT1},
    {4, 4, QS_TRAN, QS_TRAN, QS_NAT2, QS_NAT1},
    {5, 0, QS_TRAN, QS_TRAN, QS_TRAN, QS_TRAN},
    {5, 1, QS_NAT2, QS_TRAN, QS_TRAN, QS_TRAN},
    {5, 2, QS_TRAN, QS_NAT2, QS_TRAN, QS_TRAN},
    {5, 3, QS_TRAN, QS_TRAN, QS_NAT2, QS_TRAN},
    {5, 4, QS_TRAN, QS_TRAN, QS_TRAN, QS_NAT2},
    {5, 5, QS_TRAN, QS_TRAN, QS_TRAN, QS_TRAN},
};

@interface HDWQualityPolicy : NSObject

@end

@implementation HDWQualityPolicy {
    NSMutableDictionary *_quality_policies;
}

- (id)init {
    self = [super init];
    if (self) {
        _quality_policies = [NSMutableDictionary dictionary];
        
        for (int i = 0; i < sizeof(quality_policy_table)/sizeof(quality_policy_table[0]); ++i) {
            int *quality_policy_entry =  quality_policy_table[i];
            
            _quality_policies[@[@(quality_policy_entry[0]), @(quality_policy_entry[1])]] =
                @[@(quality_policy_entry[2]), @(quality_policy_entry[3]), @(quality_policy_entry[4]), @(quality_policy_entry[5])];
        }
    }
    return self;
}

- (NSArray *)qualitySetForHeights:(NSArray *)heights {
    return [_quality_policies objectForKey:heights];
}

@end

@implementation HDWQualityHelper

+ (NSMutableArray *)sortStreamDescriptors:(NSArray *)streamDescriptors {
    return [NSMutableArray arrayWithArray:[streamDescriptors sortedArrayUsingComparator:^NSComparisonResult(HDWStreamDescriptor *r1, HDWStreamDescriptor *r2) {
        if (r2.resolution.height < r1.resolution.height)
            return NSOrderedAscending;
        else if (r2.resolution.height > r1.resolution.height)
            return NSOrderedDescending;
        else
            return NSOrderedSame;
    }]];
}

+ (NSNumber *)defaultImageHeightForQuality:(NSNumber *)quality {
    NSArray *qualities = @[@240, @480, @720, @1080];
    
    if (quality.intValue >= 0 && quality.intValue < qualities.count)
        return qualities[quality.intValue];
    else
        return qualities[0];
}

+ (NSArray *)matchStreamDescriptors:(NSArray *)sourceDescriptors {
    NSMutableArray *nativeResDescriptors = [NSMutableArray array];
    NSMutableArray *transcodeResDescriptors = [NSMutableArray array];
    
    HDWStreamDescriptor *anyResDescriptor;

    for (HDWStreamDescriptor *descriptor in sourceDescriptors) {
        // Ignore invalid resolutions
        if (![descriptor.resolution.originalString isEqualToString:@"*"] && (!descriptor.resolution.height || !descriptor.resolution.width))
            continue;
        
        if (descriptor.transcoding == NO)
            [nativeResDescriptors addObject:descriptor];
        else if ([descriptor.resolution.originalString isEqualToString:@"*"]) {
            anyResDescriptor = descriptor;
        } else {
            [transcodeResDescriptors addObject:descriptor];
        }
    }
    
    nativeResDescriptors = [self sortStreamDescriptors:nativeResDescriptors];

    NSNumber *r1 = @0, *r2 = @0;
    
    if (nativeResDescriptors.count >= 1) {
        HDWStreamDescriptor *descriptor = nativeResDescriptors[0];
        r1 = [self rangeIndexForImageHeight:descriptor.resolution.height];
    }
    
    if (nativeResDescriptors.count >= 2) {
        HDWStreamDescriptor *descriptor = nativeResDescriptors[1];
        r2 = [self rangeIndexForImageHeight:descriptor.resolution.height];
    }

    HDWQualityPolicy *qualityPolicy = [[HDWQualityPolicy alloc] init];
    NSArray *qualities = [qualityPolicy qualitySetForHeights:@[r1, r2]];

    BOOL transcoding = anyResDescriptor != nil || transcodeResDescriptors.count != 0;
    NSMutableArray *descriptors = [NSMutableArray array];
    
    for (int i = 0; i < qualities.count; ++i) {
        HDWQualityMode mode = (HDWQualityMode)[qualities[i] intValue];
        HDWStreamDescriptor *descriptor = nil;
        switch (mode) {
            case QS_NAT1:
                if (!transcoding)
                    descriptor = nativeResDescriptors[0];
                else {
                    NSNumber *imageHeight = [self defaultImageHeightForQuality:@(i)];
                descriptor = [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:imageHeight.stringValue andRequestParameter:nil];
                }
                break;

            case QS_NAT2:
                if (!transcoding)
                    descriptor = nativeResDescriptors[1];
                else {
                    NSNumber *imageHeight = [self defaultImageHeightForQuality:@(i)];
                    descriptor = [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:imageHeight.stringValue andRequestParameter:nil];
                }
                break;
                
            case QS_TRAN:
                if (transcoding) {
                    NSNumber *imageHeight = [self defaultImageHeightForQuality:@(i)];
                    descriptor = [[HDWStreamDescriptor alloc] initWithProtocol:VT_MJPEG transcoding:YES resolution:imageHeight.stringValue andRequestParameter:nil];
                }
                
                break;
                
            case QS_NONE:
                break;
        }
        
        if (descriptor) {
            descriptor.quality = i;
            [descriptors addObject:descriptor];
        }
    }
    
    return [self sortStreamDescriptors:descriptors];
}

+ (NSString *)labelForQuality:(HDWQuality)quality {
    HDWOrderedDictionary *qualityLabels = [HDWOrderedDictionary dictionary];
    
    qualityLabels[@(Q_LOW)] = @"Low";
    qualityLabels[@(Q_MEDIUM)] = @"Medium";
    qualityLabels[@(Q_HIGH)] = @"High";
    qualityLabels[@(Q_BEST)] = @"Best";
    
    return L(qualityLabels[@(quality)]);
}

+ (HDWQuality)qualityForLabel:(NSString *)label {
    if ([@"Low" isEqualToString:label])
        return Q_LOW;
    else if ([@"Medium" isEqualToString:label])
        return Q_MEDIUM;
    else if ([@"High" isEqualToString:label])
        return Q_HIGH;
    else if ([@"Best" isEqualToString:label])
        return Q_BEST;
    
    return Q_UNDEFINED;
}

/**
 * Get range number for given image height.
 *
 * There are 5 ranges:
 *     0 < R1 <=  360
 *   360 < R2 <=  600
 *   600 < R3 <=  900
 *   900 < R4 <= 1080
 *  1080 < R5
 */
+ (NSNumber *)rangeIndexForImageHeight:(NSInteger)imageHeight {
    NSInteger qualityBoundaries[] = {360, 600, 900, 1080};
    
    int qualityBoundariesLength = sizeof(qualityBoundaries)/sizeof(qualityBoundaries[0]);
    for (int i = 0; i < qualityBoundariesLength; ++i) {
        NSInteger boundaryHeight = qualityBoundaries[i];
        if (imageHeight <= boundaryHeight)
            return @(i + 1);
    }
    
    return @(qualityBoundariesLength + 1);
}

+ (NSArray *)resolutionLabels:(NSArray *)streamDescriptors {
    NSMutableArray *resolutionLabels = [NSMutableArray array];
    for (HDWStreamDescriptor *streamDescriptor in streamDescriptors) {
        NSMutableString *label = [NSMutableString stringWithString:[self labelForQuality:streamDescriptor.quality]];
        if (streamDescriptor.transport == VT_HLS)
            [label appendString:@" *"];
        
        [resolutionLabels addObject:label];
    }
    return resolutionLabels;
}

@end
