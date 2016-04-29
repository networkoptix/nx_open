//
//  HDWLiveStreamingViewAdapter.h
//  HDWitness
//
//  Created by Ivan Vigasin on 4/17/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "OVTPlayerLayerView.h"
#import "HDWOptixVideoView.h"

@interface HDWLiveStreamingViewAdapter : OVTPlayerLayerView<HDWOptixVideoControl, OVTPlayerLayerViewDelegate>

@end
