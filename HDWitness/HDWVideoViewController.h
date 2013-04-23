//
//  HDWVideoViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "HDWCalendarViewController.h"
#import "HDWCameraModel.h"
#import "MotionJpegImageView.h"

@interface HDWVideoViewController : UIViewController <MotionJpegViewDelegate> {
    BOOL _playingLive;
    NSDate *_lastSelectedDate;
    
    NSDateFormatter *_dateFomatter;

    UIBarButtonItem *_liveButton;
    UIBarButtonItem *_archiveButton;
    
    __weak UIPopoverController *calendarPopover;
}

@property HDWCameraModel *camera;

@property (strong, nonatomic) IBOutlet UIScrollView *scrollView;
@property (weak, nonatomic) IBOutlet MotionJpegImageView *imageView;

@property (weak, nonatomic) IBOutlet UILabel *fpsLabel;
@property (weak, nonatomic) IBOutlet UILabel *timeLabel;

- (void)onCalendarDispose: (HDWCalendarViewController *)calendarView;

@end
