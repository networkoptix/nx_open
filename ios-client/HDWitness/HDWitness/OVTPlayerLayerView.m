//
//  OVTPlayerLayerView.m
//  VideoTest
//
//  Created by Ivan Vigasin on 4/8/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "OVTPlayerLayerView.h"

#import "HDWResolution.h"

static void *MyStreamingMovieViewControllerTimedMetadataObserverContext = &MyStreamingMovieViewControllerTimedMetadataObserverContext;
static void *MyStreamingMovieViewControllerRateObservationContext = &MyStreamingMovieViewControllerRateObservationContext;
static void *MyStreamingMovieViewControllerCurrentItemObservationContext = &MyStreamingMovieViewControllerCurrentItemObservationContext;
static void *MyStreamingMovieViewControllerPlayerItemStatusObserverContext = &MyStreamingMovieViewControllerPlayerItemStatusObserverContext;

NSString *kTracksKey		= @"tracks";
NSString *kStatusKey		= @"status";
NSString *kRateKey			= @"rate";
NSString *kPlayableKey		= @"playable";
NSString *kCurrentItemKey	= @"currentItem";
NSString *kTimedMetadataKey	= @"currentItem.timedMetadata";

#define degreesToRadians( degrees ) ( ( degrees ) / 180.0 * M_PI )

@interface OVTPlayerLayerView(Player)

- (void)assetFailedToPrepareForPlayback:(NSError *)error;
- (void)prepareToPlayAsset:(AVURLAsset *)asset withKeys:(NSArray *)requestedKeys;

@end

@implementation OVTPlayerLayerView {
    NSURL *_url;
    HDWResolution *_resolution;
    NSNumber *_angle;
    NSNumber *_aspectRatio;
    id _timeObserverId;
}

+ (Class)layerClass
{
	return [AVPlayerLayer class];
}

- (AVPlayerLayer *)playerLayer
{
	return (AVPlayerLayer *)self.layer;
}

- (void)setVideoFillMode:(NSString *)fillMode
{
	AVPlayerLayer *playerLayer = (AVPlayerLayer*)[self layer];
	playerLayer.videoGravity = fillMode;
    playerLayer.zPosition = 1;
}


- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
    }
    return self;
}

-(float)frameRate
{
    float fps=0.00;
    if (self.playerItem.asset) {
        AVAssetTrack * videoATrack = [[self.playerItem.asset tracksWithMediaType:AVMediaTypeVideo] lastObject];
        if(videoATrack) {
            fps = videoATrack.nominalFrameRate;
        }
    }
    return fps;
}

- (void)prepareToPlayAsset:(AVURLAsset *)asset withKeys:(NSArray *)requestedKeys
{
    /* Make sure that the value of each key has loaded successfully. */
	for (NSString *thisKey in requestedKeys) {
		NSError *error = nil;
		AVKeyValueStatus keyStatus = [asset statusOfValueForKey:thisKey error:&error];
		if (keyStatus == AVKeyValueStatusFailed)
		{
			[self assetFailedToPrepareForPlayback:error];
			return;
		}
		/* If you are also implementing the use of -[AVAsset cancelLoading], add your code here to bail
         out properly in the case of cancellation. */
	}
    
    /* Use the AVAsset playable property to detect whether the asset can be played. */
    if (!asset.playable) {
        /* Generate an error describing the failure. */
		NSString *localizedDescription = NSLocalizedString(@"Item cannot be played", @"Item cannot be played description");
		NSString *localizedFailureReason = NSLocalizedString(@"The assets tracks were loaded, but could not be made playable.", @"Item cannot be played failure reason");
		NSDictionary *errorDict = @{NSLocalizedDescriptionKey: localizedDescription,
								   NSLocalizedFailureReasonErrorKey: localizedFailureReason};
		NSError *assetCannotBePlayedError = [NSError errorWithDomain:@"StitchedStreamPlayer" code:0 userInfo:errorDict];
        
        /* Display the error to the user. */
        [self assetFailedToPrepareForPlayback:assetCannotBePlayedError];
        
        return;
    }
	
    /* Stop observing our prior AVPlayerItem, if we have one. */
    if (self.playerItem) {
        /* Remove existing player item key value observers and notifications. */
        
        [self.playerItem removeObserver:self forKeyPath:kStatusKey];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:AVPlayerItemDidPlayToEndTimeNotification object:self.playerItem];
    }
	
    /* Create a new instance of AVPlayerItem from the now successfully loaded AVAsset. */
    self.playerItem = [AVPlayerItem playerItemWithAsset:asset];

    self.transform = CGAffineTransformIdentity;

    if (_angle != nil && _angle.integerValue % 360)
        self.transform = CGAffineTransformRotate(self.transform, degreesToRadians(_angle.integerValue));

    if (_aspectRatio != nil && _resolution)
        self.transform = CGAffineTransformScale(self.transform, _aspectRatio.doubleValue * _resolution.height / _resolution.width, 1);

    self.layer.frame = self.window.bounds;

    /* Observe the player item "status" key to determine when it is ready to play. */
    [self.playerItem addObserver:self
                      forKeyPath:kStatusKey
                         options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                         context:MyStreamingMovieViewControllerPlayerItemStatusObserverContext];
	
    /* When the player item has played to its end time we'll toggle
     the movie controller Pause button to be the Play button */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(playerItemDidReachEnd:)
                                                 name:AVPlayerItemDidPlayToEndTimeNotification
                                               object:self.playerItem];
    
    /* Create new player, if we don't already have one. */
    if (!self.player) {
        /* Get a new AVPlayer initialized to play the specified player item. */
        [self setPlayer:[AVPlayer playerWithPlayerItem:self.playerItem]];

        double interval = .5f;
        __weak typeof (self) weakSelf = self;
        
        /* At this point we're ready to set up for playback of the asset. */
        _timeObserverId = [self.player addPeriodicTimeObserverForInterval:CMTimeMakeWithSeconds(interval, NSEC_PER_SEC) queue:nil usingBlock:^(CMTime time) {
            [weakSelf.delegate onFrameReceived:[weakSelf.playerItem.currentDate timeIntervalSince1970]  andFps:-1];
        }];
        
        /* Observe the AVPlayer "currentItem" property to find out when any
         AVPlayer replaceCurrentItemWithPlayerItem: replacement will/did
         occur.*/
        [self.player addObserver:self
                      forKeyPath:kCurrentItemKey
                         options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                         context:MyStreamingMovieViewControllerCurrentItemObservationContext];
        
        /* A 'currentItem.timedMetadata' property observer to parse the media stream timed metadata. */
        [self.player addObserver:self
                      forKeyPath:kTimedMetadataKey
                         options:0
                         context:MyStreamingMovieViewControllerTimedMetadataObserverContext];
        
        /* Observe the AVPlayer "rate" property to update the scrubber control. */
        [self.player addObserver:self
                      forKeyPath:kRateKey
                         options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                         context:MyStreamingMovieViewControllerRateObservationContext];
    }
    
    /* Make our new AVPlayerItem the AVPlayer's current item. */
    if (self.player.currentItem != self.playerItem) {
        /* Replace the player item with a new player item. The item replacement occurs
         asynchronously; observe the currentItem property to find out when the
         replacement will/did occur*/
        [[self player] replaceCurrentItemWithPlayerItem:self.playerItem];
    }
	
    [self.player play];
}

-(void)assetFailedToPrepareForPlayback:(NSError *)error {
    /* Display the error. */
	UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:[error localizedDescription]
														message:[error localizedFailureReason]
													   delegate:nil
											  cancelButtonTitle:@"OK"
											  otherButtonTitles:nil];
	[alertView show];
}

- (void)observeValueForKeyPath:(NSString*) path
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
	/* AVPlayerItem "status" property value observer. */
	if (context == MyStreamingMovieViewControllerPlayerItemStatusObserverContext)
	{
        AVPlayerStatus status = [change[NSKeyValueChangeNewKey] integerValue];
        switch (status)
        {
                /* Indicates that the status of the player is not yet known because
                 it has not tried to load new media resources for playback */
            case AVPlayerStatusUnknown: {
                NSLog(@"AVPlayerStatusUnknown");
                break;
            }
                
            case AVPlayerStatusReadyToPlay:
            {
                /* Once the AVPlayerItem becomes ready to play, i.e.
                 [playerItem status] == AVPlayerItemStatusReadyToPlay,
                 its duration can be fetched from the item. */
                
                self.playerLayer.needsDisplayOnBoundsChange = YES;
                self.playerLayer.hidden = NO;
                
                self.playerLayer.backgroundColor = [[UIColor blackColor] CGColor];
                
                /* Set the AVPlayerLayer on the view to allow the AVPlayer object to display
                 its content. */
                [self.playerLayer setPlayer:self.player];
                [self.delegate onFirstFrameReceived];
                
            }
                break;
                
            case AVPlayerStatusFailed:
            {
                AVPlayerItem *thePlayerItem = (AVPlayerItem *)object;
                [self assetFailedToPrepareForPlayback:thePlayerItem.error];
            }
                break;
        }
	}
	/* AVPlayer "currentItem" property observer.
     Called when the AVPlayer replaceCurrentItemWithPlayerItem:
     replacement will/did occur. */
	else if (context == MyStreamingMovieViewControllerCurrentItemObservationContext)
	{
        AVPlayerItem *newPlayerItem = change[NSKeyValueChangeNewKey];
        
        /* New player item null? */
        if (newPlayerItem == (id)[NSNull null])
        {
            NSLog(@"newPlayerItem == (id)[NSNull null]");
        }
        else /* Replacement of player currentItem has occurred */
        {
//            NSLog(@"Replacement of player currentItem has occurred");
            
            /* Set the AVPlayer for which the player layer displays visual output. */
            [self.playerLayer setPlayer:self.player];
            
            /* Specifies that the player should preserve the video’s aspect ratio and
             fit the video within the layer’s bounds. */
            [self setVideoFillMode:AVLayerVideoGravityResizeAspect];
        }
	}
    
    return;
}

- (void) playerItemDidReachEnd:(NSNotification*) aNotification
{
}

- (void)play {
    [self.player play];
}

- (float)volume {
    return self.player.volume;
}

- (void)setVolume:(float)volume {
    self.player.volume = volume;
}

- (void)setUrl:(NSURL *)url credential:(NSURLCredential *)credential useBasicAuth:(BOOL)useBasicAuth resolution:(HDWResolution *)resolution angle:(NSNumber *)angle andAspectRatio:(NSNumber *)aspectRatio {
    if (useBasicAuth) {
        NSString *urlString = [url.absoluteString stringByReplacingOccurrencesOfString:[NSString stringWithFormat:@"%@://", url.scheme] withString:[NSString stringWithFormat:@"%@://%@:%@@", url.scheme, credential.user, credential.password] options:NSAnchoredSearch range:NSMakeRange(0, 8)];
        _url = [NSURL URLWithString:urlString];
    } else {
        _url = url;
    }
    _resolution = resolution;
    _angle = angle;
    _aspectRatio = aspectRatio;

    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:_url options:nil];

    NSArray *requestedKeys = @[kTracksKey, kPlayableKey];
   
    __weak typeof(self) weakSelf = self;
    
    /* Tells the asset to load the values of any of the specified keys that are not already loaded. */
    [asset loadValuesAsynchronouslyForKeys:requestedKeys completionHandler:
     ^{
         dispatch_async(dispatch_get_main_queue(),
                        ^{
                            /* IMPORTANT: Must dispatch to main queue in order to operate on the AVPlayer and AVPlayerItem. */
                            [weakSelf prepareToPlayAsset:asset withKeys:requestedKeys];
                        });
     }];
}

- (void)pause {
    [self.player pause];
}

- (BOOL)isPaused {
    return self.player.rate == 0.0f;
}

- (void)stop {
    if (_timeObserverId) {
        [self.player removeTimeObserver:_timeObserverId];
    }
    
    if (self.playerItem) {
        /* Remove existing player item key value observers and notifications. */
    
        [self.playerItem removeObserver:self forKeyPath:kStatusKey];
		
        [[NSNotificationCenter defaultCenter] removeObserver:self name:AVPlayerItemDidPlayToEndTimeNotification object:self.playerItem];
    }
    
    [[NSNotificationCenter defaultCenter] removeObserver:self name:AVPlayerItemDidPlayToEndTimeNotification object:nil];
    [self.player removeObserver:self forKeyPath:kCurrentItemKey];
    [self.player removeObserver:self forKeyPath:kTimedMetadataKey];
    [self.player removeObserver:self forKeyPath:kRateKey];

    [self.playerLayer setPlayer:nil];
    
    [self.player pause];
    
    self.playerItem = nil;
    self.player = nil;
}

@end
