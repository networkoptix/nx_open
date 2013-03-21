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
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    NSURL *url = [NSURL URLWithString:@"http://admin:admin@10.0.2.133:81/videostream.cgi"];
//    NSURL *url = [NSURL URLWithString:@"http://10.0.2.187:3451/media/00-1C-A6-01-21-97.mpjpeg"];
    
    self.motionJpegImageView.url = url;
    [self.motionJpegImageView play];
	// Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
