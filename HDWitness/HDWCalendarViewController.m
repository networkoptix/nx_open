//
//  HDWCalendarViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 4/15/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWVideoViewController.h"
#import "HDWCalendarViewController.h"

@interface HDWCalendarViewController ()

@end

@implementation HDWCalendarViewController

- (void)setVideoView:(id)videoView {
    _videoView = videoView;
}

- (BOOL)liveSelected {
    return _liveSelected;
}

- (IBAction)onDone:(id)sender {
    _selectedDate = [self.datePicker date];
    [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)onCancel:(id)sender {
    [self dismissModalViewControllerAnimated:YES];
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)gotoLive: (id)sender {
    _liveSelected = YES;
    
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _liveSelected = NO;
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
        self.datePicker.date = _selectedDate;
    [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
    _selectedDate = self.datePicker.date;
    
    [(HDWVideoViewController*)_videoView onCalendarDispose:self];
    
    [super viewWillDisappear:animated];
}
@end
