//
//  HDWCalendarViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "HDWCameraModel.h"

@protocol HDWCalendarViewDelegate <NSObject>

- (void)onCalendarDateSelected:(NSTimeInterval)selectedDate;
- (void)onCalendarCancelled;

@end

@interface HDWCalendarViewController : UIViewController<UIAlertViewDelegate> {
    __weak id _videoView;
}

@property (weak, nonatomic) IBOutlet UIDatePicker *datePicker;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *goItem;
@property (weak, nonatomic) id<HDWCalendarViewDelegate> delegate;

@property (nonatomic) NSTimeInterval selectedDate;
@property (weak, nonatomic) HDWCameraModel *camera;

- (IBAction)onDone:(id)sender;

// Used in iPhone only
- (IBAction)onCancel:(id)sender;

@end
