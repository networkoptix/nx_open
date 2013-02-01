//
//  HDWDetailViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "HDWEcsConfig.h"

@interface HDWDetailViewController : UICollectionViewController <UISplitViewControllerDelegate>

@property (strong, nonatomic) HDWECSConfig* ecsConfig;
@property (strong, nonatomic) NSDictionary* servers;
@property (strong, nonatomic) NSDictionary* cameras;

@property (weak, nonatomic) IBOutlet UILabel *detailDescriptionLabel;
@end
