//
//  HDWCalendarViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWVideoViewController.h"
#import "HDWCalendarViewController.h"
#import "Config.h"

@interface HDWCalendarViewController ()

@end

@implementation HDWCalendarViewController

#pragma mark -
#pragma mark Calendar Alert

- (NSTimeInterval)dateToInterval:(NSDate *)date {
    return floor([date timeIntervalSince1970] / 60.0) * 60.0;
}

- (void)saveDateAndClose {
    _selectedDate = [self dateToInterval:[self.datePicker date]];
    [_delegate onCalendarDateSelected:_selectedDate];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex {
	if (buttonIndex == 1) {
        [self saveDateAndClose];
	}
}

- (IBAction)onDone:(id)sender {
    NSTimeInterval date = (floor([self.datePicker.date timeIntervalSince1970] / 60.0) * 60);
    if (![_camera hasChunkForExactDate:date]) {
        if ([_camera hasChunkForLaterDate:date]) {
            [[[UIAlertView alloc]
              initWithTitle: L(@"Information")
              message: L(@"There is no archive for selected date. Do you want to jump to the nearest location?")
              delegate: self
              cancelButtonTitle:L(@"No")
              otherButtonTitles:L(@"Yes"),nil] show];
        } else {
            [[[UIAlertView alloc]
              initWithTitle: L(@"Information")
              message: L(@"There have been no archive since the selected date.")
              delegate: self
              cancelButtonTitle:L(@"Ok")
              otherButtonTitles:nil, nil] show];
        }
    } else {
        [self saveDateAndClose];
    }
}

- (IBAction)onCancel:(id)sender {
    [_delegate onCalendarCancelled];
}

- (instancetype)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.goItem.title = L(@"Go");
    if (SYSTEM_VERSION_LESS_THAN(@"7.0")) {
        self.view.backgroundColor = [UIColor blackColor];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)viewDidUnload {
    [self setDatePicker:nil];
    [super viewDidUnload];
}

- (void)viewWillAppear:(BOOL)animated {
    if (self.selectedDate)
        self.datePicker.date = [NSDate dateWithTimeIntervalSince1970:_selectedDate];
    [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
}
@end
