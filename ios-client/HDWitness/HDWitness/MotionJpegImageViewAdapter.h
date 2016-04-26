//
//  MotionJpegImageViewAdapter.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/11/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "MotionJpegImageView.h"

#import "HDWOptixVideoView.h"

@interface MotionJpegImageViewAdapter : MotionJpegImageView<HDWOptixVideoControl, MotionJpegViewDelegate>

@end
