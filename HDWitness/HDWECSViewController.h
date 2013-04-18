//
//  HDWECSController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@class HDWECSConfig;

@interface HDWECSViewController : UITableViewController<UITextFieldDelegate> {
    NSArray *_dataSourceArray;
}

@property (copy) HDWECSConfig *config;
@property (copy) NSNumber *index;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *saveButtonItem;

// TODO: what is it for?
@property (nonatomic, assign) id delegate;

@end
