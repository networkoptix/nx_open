//
//  HDWECSController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@class HDWECSConfig;

@interface HDWECSViewController : UITableViewController<UITextFieldDelegate, UIAlertViewDelegate> {
    NSArray *_dataSourceArray;
}

@property (nonatomic, copy) HDWECSConfig *config;
@property (nonatomic, copy) NSNumber *index;
@property (nonatomic, weak) IBOutlet UIBarButtonItem *saveButtonItem;

@property (nonatomic, assign) id delegate;

@end
