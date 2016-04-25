//
//  HDWVideoViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "Config.h"
#import "SVProgressHud.h"
#import <MediaPlayer/MediaPlayer.h>

#import "NSString+Base64.h"
#import "NSString+MD5.h"
#import "NSString+OptixAdditions.h"

#import "BinaryChunksParser.h"
#import "HexColor.h"

#import "HDWApiConnectInfo.h"
#import "HDWCameraModel.h"
#import "HDWECSConfig.h"
#import "HDWQualityHelper.h"

#import "HDWVideoViewController.h"
#import "HDWHTTPRequestOperation.h"

#import "PopoverView_Configuration.h"

#include "version.h"
#define NOW_INTERVAL 10.0 // 10 seconds

@implementation HDWVideoViewController {
    NSInteger _streamDescriptorIndex;
}

#pragma mark -
#pragma mark ScrollViewDelegate methods

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    return self.videoView;
}

#pragma mark -
#pragma mark HDWOptixVideoViewDelegate methods

- (void)onFirstFrameReceived {
    _firstFrameReceived = YES;
    self.qualityButton.hidden = NO;
    self.timeLabel.text = @"";
    [SVProgressHUD dismiss];
}

- (void)onFrameReceived:(NSTimeInterval)timestamp andFps:(NSInteger)currentFps {
    if (!_firstFrameReceived)
        return;
    
    _lastFrameTimestamp = timestamp;
    
    if (fabs(timestamp) > 1)
        self.timeLabel.text = [_dateFormatter stringFromDate:[NSDate dateWithTimeIntervalSince1970:timestamp]];
    
    self.nameLabel.text = self.camera.name;
}

- (void)loadChunks {
    HDWECSModel *ecs = self.camera.server.ecs;
    NSURLCredential *credential = ecs.config.credential;
    if (![ecs isCameraArchive:self.camera accessibleToUser:ecs.config.login]) {
        self.archiveButton.enabled = NO;
        return;
    }
    
    __weak typeof(self) weakSelf = self;
    
    NSOperationQueue *operationQueue = [[NSOperationQueue alloc] init];
    
    NSEnumerator *valueEnumerator = [ecs.servers valuesEnumerator];
    HDWServerModel *server;
    while (server = [valueEnumerator nextObject]) {
        NSURL *chunksUrl = [_protocol chunksUrlForCamera:self.camera atServer:server];
        
        NSMutableURLRequest *chunksRequest = [NSMutableURLRequest requestWithURL:chunksUrl cachePolicy:NSURLRequestReloadIgnoringLocalAndRemoteCacheData timeoutInterval:20.0];
        
        if ([_protocol useBasicAuth]) {
            NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", credential.user, credential.password];
            [chunksRequest addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
        }
        
        AFHTTPRequestOperation *requestOperation;
        requestOperation = [[AFHTTPRequestOperation alloc] initWithRequest:chunksRequest];
        requestOperation.allowsInvalidSSLCertificate = YES;
        [requestOperation setCompletionBlockWithSuccess:^(AFHTTPRequestOperation *operation, id responseObject) {
            if ([responseObject length] <=3) {
                DDLogVerbose(@"loadChunks: Got too short response: length is %d. Ignoring.", [responseObject length]);
                return;
            }
            NSData *data = [responseObject subdataWithRange:NSMakeRange(3, [responseObject length] - 3)];
            NSMutableArray *chunksArray = [NSMutableArray array];
            decodeBinaryChunks(data.bytes, (int)data.length, chunksArray);
            [weakSelf.camera setChunks:[[HDWChunks alloc] initWithArray:chunksArray] forServer:server];
            
            weakSelf.archiveButton.enabled = [weakSelf.camera hasChunks];
        } failure:nil];
        [operationQueue addOperation:requestOperation];
    }
}

- (void)useProtocol:(id<HDWProtocol>)protocol cameraId:(NSString *)cameraUniqueId andDelegate:(id<HDWCameraAccessor>)delegate {
    _protocol = protocol;
    _cameraUniqueId = cameraUniqueId;
    _delegate = delegate;
    [self loadChunks];
}

- (HDWCameraModel *)camera {
    return [_delegate cameraForUniqueId:_cameraUniqueId];
}

- (IBAction)onPauseItemClick:(UIBarButtonItem *)sender {
    if (self.videoView.isPaused) {
        [self updatePauseButton:YES];
        
        [self restoreVideo];
    } else {
        [self updatePauseButton:NO];
        [self.videoView pause];
    }
    
    [self updateControls];
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
    return YES;
    
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation {
    self.scrollView.contentSize = self.scrollView.frame.size;
    self.videoView.frame = CGRectMake(0, 0, self.scrollView.frame.size.width, self.scrollView.frame.size.height);
}

- (instancetype)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
    }
    return self;
}

- (void)showTimeSelector: (id)sender {
    [self.videoView stop];
    
    [self performSegueWithIdentifier:@"setTime" sender:self];
}

- (void)_updateLabels {
    if (_playingLive) {
        [self updatePauseButton:YES];
        _startStopButton.enabled = NO;
        self.timeLabel.textColor = [UIColor whiteColor];
        self.nameLabel.textColor = [UIColor whiteColor];
    } else {
        _startStopButton.enabled = YES;
        self.timeLabel.textColor = [UIColor redColor];
        self.nameLabel.textColor = [UIColor redColor];
    }
}

- (void)setPlayingLive:(BOOL)playingLive {
    _playingLive = playingLive;
    [self updateControls];
}

- (void)updatePauseButton:(BOOL)pause {
    UIBarButtonSystemItem systemItem = pause ? UIBarButtonSystemItemPause : UIBarButtonSystemItemPlay;
    _startStopButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:systemItem target:self action:@selector(onPauseItemClick:)];
    [_startStopButton setTintColor:[UIColor colorWithHexString:@ QN_IOS_PLAYBUTTON_TINT alpha:1]];
    NSMutableArray *items = [NSMutableArray arrayWithArray:self.toolbar.items];
    
    // 0,2 are spacers, 1 is play/pause, 3 is quality
    [items setObject:_startStopButton atIndexedSubscript:1];
    [self.toolbar setItems:items];
}

- (void)updateControls {
    self.scrollView.maximumZoomScale = self.camera.onlineOrRecording ? 4.0 : 1.0;

    [_liveButton setEnabled:(!_playingLive && (self.camera.calculatedStatus.intValue == Status_Online || self.camera.calculatedStatus.intValue == Status_Recording))];
    
    [self _updateLabels];
}

- (void)gotoURL:(NSURL*)url withCredential:(NSURLCredential *)credential {
    if (![_protocol useBasicAuth])
        [HDWHTTPRequestOperation setAuthCookiesForConfig:self.camera.server.ecs.config];
    
    _firstFrameReceived = NO;
    [self updateControls];
    
    HDWResolution *resolution = 0;
    if (_streamDescriptorIndex >= 0)
        resolution = [_streamDescriptors[_streamDescriptorIndex] resolution];
    
    self.scrollView.zoomScale = 1.0;
    [self.videoView setUrl:url credential:credential useBasicAuth:[_protocol useBasicAuth] resolution:resolution angle:self.camera.angle andAspectRatio:self.camera.aspectRatio];
    [self.videoView play];
    
    [SVProgressHUD showWithStatus:L(@"Loading...")];
}

- (void)gotoLive:(id)sender {
    [self setPlayingLive:YES];
    if (self.camera.calculatedStatus.intValue == Status_Offline) {
        [SVProgressHUD showWithStatus:L(@"No Signal...")];
    } else if (self.camera.calculatedStatus.intValue == Status_Unauthorized) {
        [SVProgressHUD showWithStatus:L(@"Invalid Camera Credentials...")];
    } else {
        NSURL *url = [_protocol liveUrlForCamera:self.camera andStreamDescriptor:self.streamDescriptor];
        NSURLCredential *credential = self.camera.server.ecs.config.credential;
        [self gotoURL:url withCredential:credential];
    }
}

- (void)gotoDate:(NSTimeInterval)date {
    NSTimeInterval timeSinceNow = date - [[NSDate date] timeIntervalSince1970];
    if (timeSinceNow > 0 || -timeSinceNow < NOW_INTERVAL) {
        [self gotoLive:self];
    } else {
        [self setPlayingLive:NO];
        NSURL *url = [_protocol archiveUrlForCamera:self.camera date:date andStreamDescriptor:self.streamDescriptor];
        NSURLCredential *credential = self.camera.server.ecs.config.credential;
        [self gotoURL:url withCredential:credential];
    }
}

#pragma mark -
#pragma mark Date Selection

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
        
        calendarViewController.camera = self.camera;
        calendarViewController.delegate = self;
    }
}

- (void)onCalendarDateSelected: (NSTimeInterval)selectedDate {
    _lastSelectedDate = selectedDate;
    
    if (calendarPopover) {
        [self gotoDate:_lastSelectedDate];
        [calendarPopover dismissPopoverAnimated:YES];
    } else
        [self.navigationController dismissViewControllerAnimated:YES completion:^{
            [self gotoDate:_lastSelectedDate];
        }];
}

- (void)onCalendarCancelled {
    if (_playingLive)
        [self gotoLive:self];
    else
        [self gotoDate:_lastSelectedDate];
    
    [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

- (PopoverView *)showPopoverAtPoint:(CGPoint)point inView:(UIView *)view withStringArray:(NSArray *)stringArray active:(NSInteger)active andDelegate:(id<PopoverViewDelegate>)delegate {
    PopoverView *popoverView = [[PopoverView alloc] initWithFrame:CGRectZero];
    
    NSMutableArray *labelArray = [[NSMutableArray alloc] initWithCapacity:stringArray.count];
    
    UIFont *plainFont = kTextFont;
    UIFont *boldFont = [UIFont fontWithName:@"HelveticaNeue-Bold" size:16.f];
    
    int n = 0;
    for (NSString *string in stringArray) {
        UIFont *font = (active == n) ? boldFont : plainFont;
        CGSize textSize = [string sizeWithFont:font];
        UIButton *textButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, textSize.width, textSize.height)];
        textButton.backgroundColor = [UIColor clearColor];
        textButton.titleLabel.font = font;
        textButton.titleLabel.textAlignment = kTextAlignment;
        textButton.titleLabel.textColor = kTextColor;
        [textButton setTitle:string forState:UIControlStateNormal];
        textButton.layer.cornerRadius = 4.f;
        [textButton setTitleColor:kTextColor forState:UIControlStateNormal];
        [textButton setTitleColor:kTextHighlightColor forState:UIControlStateHighlighted];
        [textButton addTarget:popoverView action:@selector(didTapButton:) forControlEvents:UIControlEventTouchUpInside];
        
        [labelArray addObject:textButton];
        n++;
    }
    
    [popoverView showAtPoint:point inView:view withViewArray:labelArray];
    
    popoverView.delegate = delegate;
    return popoverView;

}

#pragma mark -
#pragma mark Actions

- (IBAction)onQualityClick:(UIButton *)sender {
    if (_qualityPopover) {
        [_qualityPopover dismiss];
        return;
    }
    
    _qualityPopover = [self showPopoverAtPoint:CGPointMake(self.view.bounds.size.width - self.qualityButton.frame.size.width, self.view.bounds.size.height - self.qualityButton.frame.size.height)
                 inView:self.view
                 withStringArray:_streamDescriptorLabels
                 active:_streamDescriptorIndex
                 andDelegate:self];
}

- (HDWStreamDescriptor *)streamDescriptor {
    if (_streamDescriptorIndex == -1)
        return nil;
    
    return (HDWStreamDescriptor *)_streamDescriptors[_streamDescriptorIndex];
}

- (void)popoverView:(PopoverView *)popoverView didSelectItemAtIndex:(NSInteger)index {
    _streamDescriptorIndex = index;
    
    [popoverView dismiss];
    
    if (!self.videoView.isPaused)
        [self restoreVideo];
}

- (void)popoverViewDidDismiss:(PopoverView *)popoverView {
    _qualityPopover = nil;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    _resolutionPreference = [defaults objectForKey:@"resolution_preference"];

    self.scrollView.delegate = self;
    self.scrollView.clipsToBounds = YES;
    
    self.scrollView.zoomScale = 1.0;
    self.scrollView.minimumZoomScale = 1.0;
    self.scrollView.maximumZoomScale = 4.0;

    self.qualityButton.hidden = YES;
    _qualityPopover = nil;
    
    _streamDescriptorLabels = [HDWQualityHelper resolutionLabels:_streamDescriptors];
    int preferredQuality = [HDWQualityHelper qualityForLabel:_resolutionPreference];
    if (_streamDescriptors && _streamDescriptors.count > 0) {
        _streamDescriptorIndex = _streamDescriptors.count - 1;
        for (int n = 0; n < _streamDescriptors.count ; n++) {
            if ([[_streamDescriptors objectAtIndex:n] quality] <= preferredQuality) {
                _streamDescriptorIndex = n;
                break;

            }
        }
    } else {
        _streamDescriptorIndex = -1;
        NSLog(@"No stream descriptors!");
    }

    _dateFormatter = [[NSDateFormatter alloc] init];
    [_dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss"];

    self.videoView.delegate = self;
    _liveButton = [[UIBarButtonItem alloc] initWithTitle:L(@"Live") style:UIBarButtonItemStylePlain target:self action:@selector(gotoLive:)];
   
    _archiveButton.title = L(@"Archive");
    
    self.navigationItem.rightBarButtonItems = @[_liveButton, _archiveButton];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onAppDidEnterBackground:)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onAppWillEnterForeground:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    
    [self gotoLive:self];

    UITapGestureRecognizer *singleTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
    singleTap.numberOfTapsRequired = 1;
    singleTap.delegate = self;
    [self.videoView addGestureRecognizer:singleTap];
}

- (void)handleTap:(UIGestureRecognizer *)gestureRecognizer {
    CGFloat startAlpha, endAlpha;
    
    if (fabs(self.toolbar.alpha) <= 1e-3) {
        startAlpha = 0.0f;
        endAlpha = 1.0f;
    } else if (fabs(self.toolbar.alpha - 1) <= 1e-3) {
        startAlpha = 1.0f;
        endAlpha = 0.0f;
    } else {
        return;
    }
    
    self.toolbar.alpha = startAlpha;
    [UIView animateWithDuration:1.5 animations:^() {
        self.toolbar.alpha = endAlpha;
    }];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer {
    return YES;
}

- (void)dealloc {
    self.videoView.delegate = nil;
    [self.videoView stop];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
 
}

// Dispose of any resources that can be recreated.

- (void)onAppDidEnterBackground:(NSNotification *)notification {
    [self.videoView pause];
}

- (void)restoreVideo {
    if (_playingLive)
        [self gotoLive:self];
    else
        [self gotoDate:_lastFrameTimestamp];
}

- (void)onAppWillEnterForeground:(NSNotification *)notification {
    [self restoreVideo];
}

#pragma mark -
#pragma mark UIView

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    
    [self updatePauseButton:YES];
    [self updateControls];
    
    self.scrollView.contentSize = self.scrollView.frame.size;
    self.videoView.frame = CGRectMake(0, 0, self.scrollView.frame.size.width, self.scrollView.frame.size.height);
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    
    [calendarPopover dismissPopoverAnimated:YES];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    
    [self.videoView stop];
    [SVProgressHUD dismiss];
}

- (void)viewDidUnload {
    [self setQualityButton:nil];
    [self setArchiveButton:nil];
    [self setNameLabel:nil];
    [super viewDidUnload];
}

@end
