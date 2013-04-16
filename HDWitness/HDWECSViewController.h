//
//  HDWECSController.h
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import <UIKit/UIKit.h>

@class HDWECSConfig;

@interface HDWECSViewController : UITableViewController {
}

-(id) initWithConfig: (HDWECSConfig*)config;

@property (copy) HDWECSConfig *config;
@property (nonatomic, retain) NSArray *dataSourceArray;
@property (nonatomic, assign) id delegate;

@end
