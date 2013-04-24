//
//  HDWVideoViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "SVProgressHud.h"
#import "HDWCameraModel.h"
#import "HDWVideoViewController.h"

#define NOW_INTERVAL 10.0 // 10 seconds

@interface HDWVideoViewController ()

@end

@implementation HDWVideoViewController

- (void)onFirstFrameReceived {
    _firstFrameReceived = YES;
    [SVProgressHUD dismiss];
}

- (void)onFrameReceived:(NSDate*)timestamp andFps:(NSInteger)currentFps {
    _lastFrameTimestamp = timestamp;
    
    self.fpsLabel.text = [NSString stringWithFormat:@"FPS: %d", currentFps];
    self.timeLabel.text = [_dateFomatter stringFromDate:timestamp];
}

- (void)onConnectionClosed {
    if (!_playingLive && !_firstFrameReceived)
        [self gotoLive:self];
}

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
    _firstFrameReceived = NO;
    [self updateControls];
    
    self.imageView.username = url.user;
    self.imageView.password = url.password;
    self.imageView.url = url;
    [self.imageView play];
    [SVProgressHUD showWithStatus:@"Loading..."];
}

- (void)gotoLive:(id)sender {
    _playingLive = YES;
    if (_camera.status.intValue == Status_Offline) {
        [SVProgressHUD showWithStatus:@"No Signal..."];
    } else if (_camera.status.intValue == Status_Unauthorized) {
        [SVProgressHUD showWithStatus:@"Invalid Camera Credentials..."];
    } else {
        [self gotoURL:[_camera liveUrl]];
    }
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
        calendarViewController.delegate = self;
    }
}

- (void)onCalendarDispose: (NSDate *)selectedDate {
    _lastSelectedDate = selectedDate;
    [self gotoDate:_lastSelectedDate];
}

- (void)viewDidLoad {
    [super viewDidLoad];

    _dateFomatter = [[NSDateFormatter alloc] init];
    [_dateFomatter setDateFormat:@"yyyy-MM-dd HH:mm:ss"];

    self.imageView.delegate = self;
    _liveButton = [[UIBarButtonItem alloc] initWithTitle:@"Live" style:UIBarButtonItemStylePlain target:self action:@selector(gotoLive:)];
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
 
}    // Dispose of any resources that can be recreated.

- (void)onAppDidEnterBackground:(NSNotification *)notification {
    [self.imageView pause];
}

- (void)onAppWillEnterForeground:(NSNotification *)notification {
    if (_playingLive)
        [self gotoLive:self];
    else
        [self gotoDate:_lastFrameTimestamp];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    
    [self setFpsLabel:nil];
    [self setTimeLabel:nil];
    [self.imageView stop];
    [SVProgressHUD dismiss];
}

@end
