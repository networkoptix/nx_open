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
#import "HDWEventSource.h"
#import "HDWProtocol.h"
#import "HDWCameraAccessor.h"

@interface HDWDetailViewController : UICollectionViewController <UISplitViewControllerDelegate, HDWEventSourceDelegate, UIAlertViewDelegate, UICollectionViewDelegate,HDWCameraAccessor> {
    BOOL _collectionViewUpdatesDisabled;
    
    id<HDWProtocol> _protocol;
}

@property (strong, nonatomic) HDWECSConfig *ecsConfig;
@property (strong, nonatomic) HDWEventSource *messageSource;

@property (weak, nonatomic) IBOutlet UIBarButtonItem *hideOfflineButtonItem;
- (IBAction)onHideOffline:(UIBarButtonItem *)sender;

@property (strong, nonatomic) UIPopoverController *masterPopoverController;

@end
