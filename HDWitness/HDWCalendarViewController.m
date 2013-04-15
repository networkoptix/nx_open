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

- (NSDate*)selectedDate {
    return [self.datePicker date];
}

- (BOOL)liveSelected {
    return _liveSelected;
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
//    [self performSegueWithIdentifier:@"unwindCalendar" sender:self];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _liveSelected = NO;
    
    UIBarButtonItem *archiveButton =
        [[UIBarButtonItem alloc] initWithTitle:@"Live" style:UIBarButtonItemStylePlain target:self action:@selector(gotoLive:)];
    self.navigationItem.rightBarButtonItem = archiveButton;
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

-(void) viewWillDisappear:(BOOL)animated {
    if ([self.navigationController.viewControllers indexOfObject:self]==NSNotFound) {
        HDWVideoViewController* videoViewController = [self.navigationController.viewControllers lastObject];
        [videoViewController onCalendarDispose:self];
        // back button was pressed.  We know this is true because self is no longer
        // in the navigation stack.
    }
    [super viewWillDisappear:animated];
}
@end
