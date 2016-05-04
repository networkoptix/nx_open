//
//  HDWMasterViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@class HDWDetailViewController;
@class HDWECSConfig;

@interface HDWMasterViewController : UITableViewController

@property (strong, nonatomic) HDWDetailViewController *detailViewController;

- (void)addOrReplaceECSConfig:(HDWECSConfig*)ecsConfig atIndex:(NSUInteger)index;
- (NSUInteger)countOtherECSWithSameName:(NSString*)name atIndex:(NSUInteger)index;

@end
