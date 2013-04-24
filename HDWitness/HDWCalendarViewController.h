//
//  HDWCalendarViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol HDWCalendarViewDelegate <NSObject>

- (void)onCalendarDispose:(NSDate *)selectedDate;

@end

@interface HDWCalendarViewController : UIViewController {
    BOOL _liveSelected;
    id _videoView;
}

@property (weak, nonatomic) IBOutlet UIDatePicker *datePicker;
@property NSDate* selectedDate;
@property (nonatomic, assign) id<HDWCalendarViewDelegate> delegate;

- (BOOL)liveSelected;

- (IBAction)onDone:(id)sender;
- (IBAction)onCancel:(id)sender;

@end
