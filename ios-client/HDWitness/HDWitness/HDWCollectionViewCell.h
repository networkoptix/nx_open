//
//  HDWCollectionViewCell.h
//  HDWitness
//
//  Created by Ivan Vigasin on 2/1/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

// #import <PSTCollectionView/PSTCollectionView.h>
#import "FXImageView.h"

@interface HDWCollectionViewCell : UICollectionViewCell

@property (weak, nonatomic) IBOutlet FXImageView *imageView;
@property (weak, nonatomic) IBOutlet UILabel *labelView;

@end