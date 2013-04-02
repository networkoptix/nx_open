//
//  HDWDetailViewController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "HDWCameraModel.h"
#import "HDWVideoViewController.h"
#import "SRWebSocket.h"

@interface HDWDetailViewController : UICollectionViewController <UISplitViewControllerDelegate, SRWebSocketDelegate>

@property (strong, nonatomic) HDWECSConfig *ecsConfig;
@property (strong, nonatomic) HDWECSModel *ecsModel;

@property (strong, nonatomic) SRWebSocket* socket;
@property (weak, nonatomic) IBOutlet UILabel *detailDescriptionLabel;
@property (strong, nonatomic) HDWVideoViewController *videoViewController;
@end
