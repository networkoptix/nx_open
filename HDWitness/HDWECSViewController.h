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
    HDWECSConfig *item;
}

-(id) initWithConfig: (HDWECSConfig*)config;
-(void) setConfig: (HDWECSConfig*)config;

@property (nonatomic, retain) NSArray *dataSourceArray;

@end
