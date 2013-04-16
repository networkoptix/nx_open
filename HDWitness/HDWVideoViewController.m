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

//- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
//{
//    return self.imageView;
//}

- (void)showTimeSelector: (id)sender {
    [self.imageView stop];
    
    [self performSegueWithIdentifier:@"setTime" sender:self];
}

- (void)onCalendarDispose: (HDWCalendarViewController *)calendarView {
    NSURL *url;
    
    if ([calendarView liveSelected])
        url = [_camera liveUrl];
    else {
        if (fabs([calendarView.selectedDate timeIntervalSinceNow]) < NOW_INTERVAL)
            url = [_camera liveUrl];
        else
            url = [_camera archiveUrlForDate:calendarView.selectedDate];
    }
    
    self.imageView.url = url;
    [self.imageView play];
}

- (void)viewDidLoad {
    [super viewDidLoad];

    UIBarButtonItem *archiveButton =
        [[UIBarButtonItem alloc] initWithTitle:@"Archive" style:UIBarButtonItemStylePlain target:self action:@selector(showTimeSelector:)];
    self.navigationItem.rightBarButtonItem = archiveButton;

    self.imageView.allowSelfSignedCertificates = YES;
    self.imageView.allowClearTextCredentials = YES;
    
//    self.scrollView.minimumZoomScale=0.5;
//    self.scrollView.maximumZoomScale=6.0;
//    self.scrollView.contentSize=CGSizeMake(1280, 720);
//    self.scrollView.delegate=self;
    
    NSURL *liveUrl = [_camera liveUrl];
    self.imageView.username = liveUrl.user;
    self.imageView.password = liveUrl.password;
    self.imageView.url = liveUrl;
    [self.imageView play];
    
	// Do any additional setup after loading the view.
}

- (void)dealloc {
    [self.imageView stop];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
