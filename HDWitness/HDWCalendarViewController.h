//
//  HDWCalendarViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface HDWCalendarViewController : UIViewController {
    BOOL _liveSelected;
}

@property (weak, nonatomic) IBOutlet UIDatePicker *datePicker;
@property (readonly) NSDate* selectedDate;

- (BOOL)liveSelected;

@end
