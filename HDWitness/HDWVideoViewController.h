//
//  HDWVideoViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "HDWCameraModel.h"
#import "MotionJpegImageView.h"

@interface HDWVideoViewController : UIViewController

@property HDWCameraModel *camera;

@property (weak, nonatomic) IBOutlet MotionJpegImageView *motionJpegImageView;

@end
