//
//  HDWVideoViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWVideoViewController.h"

#define NOW_INTERVAL 10.0 // 10 seconds

@interface HDWVideoViewController ()

@end

@implementation HDWVideoViewController

- (BOOL)shouldAutorotate {
    return YES;
}


- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAll;
}


- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeLeft;
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
    if (orientation == UIInterfaceOrientationPortraitUpsideDown) {
        return NO;
    }
    
    return YES;
    
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
    }
    return self;
}

- (void)showTimeSelector: (id)sender {
    [self.imageView stop];
    
    [self performSegueWithIdentifier:@"setTime" sender:self];
}

- (void)updateControls {
    [_liveButton setEnabled:!_playingLive];
}

- (void)gotoURL:(NSURL*)url {
    [self updateControls];
    
    self.imageView.username = url.user;
    self.imageView.password = url.password;
    self.imageView.url = url;
    [self.imageView play];
}

- (void)gotoLive:(id)sender {
    _playingLive = YES;
    [self gotoURL:[_camera liveUrl]];
}

- (void)gotoDate:(NSDate*)date {
    NSTimeInterval timeSinceNow = [date timeIntervalSinceNow];
    if (timeSinceNow > 0 || -timeSinceNow < NOW_INTERVAL) {
        [self gotoLive:self];
    } else {
        _playingLive = NO;
        [self gotoURL:[_camera archiveUrlForDate:date]];
    }
}

- (IBAction)showPopover:(id)sender {
    if (calendarPopover)
        [calendarPopover dismissPopoverAnimated:YES];
    else
        [self performSegueWithIdentifier:@"setTime" sender:sender];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([segue.identifier isEqualToString:@"setTime"]) {
        if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad)
            calendarPopover = [(UIStoryboardPopoverSegue*)segue popoverController];
        
        UINavigationController *navigationViewController = segue.destinationViewController;
        HDWCalendarViewController *calendarViewController = (HDWCalendarViewController *)navigationViewController.topViewController;
        if (_lastSelectedDate) {
            [calendarViewController setSelectedDate:_lastSelectedDate];
        }
        [calendarViewController setVideoView: self];
    }
}

- (void)onCalendarDispose: (HDWCalendarViewController *)calendarView {
    _lastSelectedDate = calendarView.selectedDate;
    [self gotoDate:_lastSelectedDate];
}

- (void)viewDidLoad {
    [super viewDidLoad];

    _liveButton = [[UIBarButtonItem alloc] initWithTitle:@"Live" style:UIBarButtonItemStylePlain target:self action:@selector(gotoLive:)];
//    _archiveButton = [[UIBarButtonItem alloc] initWithTitle:@"Archive" style:UIBarButtonItemStylePlain target:self action:@selector(showTimeSelector:)];
    _archiveButton = self.navigationItem.rightBarButtonItem;
   
    self.navigationItem.rightBarButtonItems = @[_liveButton, _archiveButton];

    self.imageView.allowSelfSignedCertificates = YES;
    self.imageView.allowClearTextCredentials = YES;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onAppDidEnterBackground:)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onAppWillEnterForeground:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    
    [self gotoLive:self];
}

- (void)dealloc {
    [self.imageView stop];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)onAppDidEnterBackground:(NSNotification *)notification {
    [self.imageView pause];
}

- (void)onAppWillEnterForeground:(NSNotification *)notification {
    [self.imageView play];
}
@end
