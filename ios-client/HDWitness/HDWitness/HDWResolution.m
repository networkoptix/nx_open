//
//  HDWResolution.m
//  HDWitness
//
//  Created by Ivan Vigasin on 2/13/15.
//  Copyright (c) 2015 Ivan Vigasin. All rights reserved.
//

#import "HDWResolution.h"

@implementation HDWResolution {
    int _width;
    int _height;
    NSString *_originalString;
}

- (instancetype)initWithString:(NSString *)resolutionString {
    self = [super init];
    if (self) {
        _originalString = resolutionString;
        _width = _height = 0;
        
        NSArray *resolutionComponents = [resolutionString componentsSeparatedByString:@"x"];
        if (resolutionComponents.count == 1)
            _height = [resolutionComponents.firstObject intValue];
        else if (resolutionComponents.count > 1) {
            _width = [resolutionComponents.firstObject intValue];
            _height = [resolutionComponents[1] intValue];
        }
    }
    
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%dx%d", _width, _height];
}

@end
