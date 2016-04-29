//
//  HDWQualityHelper.h
//  HDWitness
//
//  Created by Ivan Vigasin on 07/09/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "HDWCameraModel.h"

@interface HDWQualityHelper : NSObject

+ (NSArray *)matchStreamDescriptors:(NSArray *)sourceDescriptors;
+ (NSArray *)resolutionLabels:(NSArray *)resolutions;
+ (HDWQuality)qualityForLabel:(NSString *)label;

@end
