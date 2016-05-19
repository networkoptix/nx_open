//
//  HDWVideoViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "PopoverView/PopoverView.h"
#import "AFNetworking/AFHTTPRequestOperation.h"

#import "HDWCameraModel.h"
#import "HDWProtocol.h"
#import "HDWCalendarViewController.h"
#import "HDWOptixVideoView.h"
#import "HDWCameraAccessor.h"

@interface HDWVideoViewController : UIViewController <HDWOptixVideoViewDelegate, HDWCalendarViewDelegate,PopoverViewDelegate, UIScrollViewDelegate, UIGestureRecognizerDelegate> {
    
    id<HDWProtocol> _protocol;
    
    BOOL _playingLive;
    BOOL _firstFrameReceived;
    
    NSTimeInterval _lastFrameTimestamp;
    NSTimeInterval _lastSelectedDate;
   
    NSArray *_streamDescriptorLabels;
    
    NSDateFormatter *_dateFormatter;

    UIBarButtonItem *_liveButton;
    UIBarButtonItem *_startStopButton;
    PopoverView *_qualityPopover;

    NSString *_cameraUniqueId;
    NSString *_resolutionPreference;
    id<HDWCameraAccessor> _delegate;
    __weak UIPopoverController *calendarPopover;
}

@property (nonatomic) NSArray *streamDescriptors;
@property (weak, nonatomic) IBOutlet UIToolbar *toolbar;
@property (weak, nonatomic) IBOutlet UIScrollView *scrollView;
@property (weak, nonatomic) IBOutlet HDWOptixVideoView *videoView;

@property (weak, nonatomic) IBOutlet UILabel *timeLabel;
@property (weak, nonatomic) IBOutlet UILabel *nameLabel;
@property (weak, nonatomic) IBOutlet UIButton *qualityButton;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *archiveButton;

- (void)useProtocol:(id<HDWProtocol>)connectInfo cameraId:(NSString *)cameraUniqueId andDelegate:(id<HDWCameraAccessor>)delegate;
- (IBAction)onPauseItemClick:(UIBarButtonItem *)sender;
- (IBAction)onQualityClick:(UIButton *)sender;
- (void)onCalendarDateSelected:(NSTimeInterval)selectedDate;

@end
