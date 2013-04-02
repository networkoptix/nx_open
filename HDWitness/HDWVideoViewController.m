//
//  HDWVideoViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 3/20/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWVideoViewController.h"

@interface HDWVideoViewController ()

@end

@implementation HDWVideoViewController

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

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.imageView.allowSelfSignedCertificates = YES;
    self.imageView.allowClearTextCredentials = YES;
    
//    self.scrollView.minimumZoomScale=0.5;
//    self.scrollView.maximumZoomScale=6.0;
//    self.scrollView.contentSize=CGSizeMake(1280, 720);
//    self.scrollView.delegate=self;
    
    //    NSURL *url = [NSURL URLWithString:@"http://admin:admin@10.0.2.133:81/videostream.cgi"];
//    NSURL *url = [NSURL URLWithString:@"http://10.0.2.187:3451/media/00-1C-A6-01-21-97.mpjpeg"];
    
    NSLog(@"Video Url: %@", _camera.videoUrl.absoluteString);
    self.imageView.username = _camera.videoUrl.user;
    self.imageView.password = _camera.videoUrl.password;
    self.imageView.url = _camera.videoUrl;
    [self.imageView play];
	// Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
