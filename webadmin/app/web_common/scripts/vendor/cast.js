var PLAYER_STATE = {
    IDLE: 'IDLE',
    LOADING: 'LOADING',
    LOADED: 'LOADED',
    PLAYING: 'PLAYING',
    PAUSED: 'PAUSED',
    STOPPED: 'STOPPED',
    ERROR: 'ERROR'
};

/**
 * Cast player object
 * Main variables:
 *  - PlayerHandler object for handling media playback
 *  - Cast player variables for controlling Cast mode media playback
 *  - Current media variables for transition between Cast and local modes
 * @struct @constructor
 */
var CastPlayer = function() {
    /** @type {PlayerHandler} Delegation proxy for media playback */
    this.playerHandler = new PlayerHandler(this);

    /** @type {PLAYER_STATE} A state for media playback */
    this.playerState = PLAYER_STATE.IDLE;

    /* Cast player variables */
    /** @type {cast.framework.RemotePlayer} */
    this.remotePlayer = null;
    /** @type {cast.framework.RemotePlayerController} */
    this.remotePlayerController = null;

    /* Current media variables */
    /** @type {number} A number for current media index */
    this.currentMediaIndex = 0;
    /** @type {number} A number for current media time */
    this.currentMediaTime = 0;
    /** @type {number} A number for current media duration */
    this.currentMediaDuration = -1;
    /** @type {?number} A timer for tracking progress of media */
    this.timer = null;

    /** @type {boolean} Fullscreen mode on/off */
    this.fullscreen = false;

    /** @type {function()} */
    //this.incrementMediaTimeHandler = this.incrementMediaTime.bind(this);
};

CastPlayer.prototype.initializeCastPlayer = function() {
    var options = {};

    // Set the receiver application ID to your own (created in the
    // Google Cast Developer Console), or optionally
    // use the chrome.cast.media.DEFAULT_MEDIA_RECEIVER_APP_ID
    options.receiverApplicationId = /*chrome.cast.media.DEFAULT_MEDIA_RECEIVER_APP_ID;/*/'D9E6D045';

    // Auto join policy can be one of the following three:
    // ORIGIN_SCOPED - Auto connect from same appId and page origin
    // TAB_AND_ORIGIN_SCOPED - Auto connect from same appId, page origin, and tab
    // PAGE_SCOPED - No auto connect
    options.autoJoinPolicy = chrome.cast.AutoJoinPolicy.ORIGIN_SCOPED;


    cast.framework.CastContext.getInstance().setOptions(options);

    this.remotePlayer = new cast.framework.RemotePlayer();
    this.remotePlayerController = new cast.framework.RemotePlayerController(this.remotePlayer);
    this.remotePlayerController.addEventListener(
        cast.framework.RemotePlayerEventType.IS_CONNECTED_CHANGED,
        this.switchPlayer.bind(this)
    );
    this.remotePlayerController.addEventListener(
        cast.framework.RemotePlayerEventType.ANY_CHANGE,
        function(event){
            //console.log(event);
        }
    );
};

/*
 * PlayerHandler and setup functions
 */

CastPlayer.prototype.switchPlayer = function() {
    //this.stopProgressTimer();
    this.playerHandler.stop();
    this.playerState = PLAYER_STATE.IDLE;
    if (cast && cast.framework) {
        if (this.remotePlayer.isConnected) {
            this.setupRemotePlayer();
            return;
        }
    }
};

/**
 * PlayerHandler
 *
 * This is a handler through which the application will interact
 * with both the RemotePlayer and LocalPlayer. Combining these two into
 * one interface is one approach to the dual-player nature of a Cast
 * Chrome application. Otherwise, the state of the RemotePlayer can be
 * queried at any time to decide whether to interact with the local
 * or remote players.
 *
 * To set the player used, implement the following methods for a target object
 * and call setTarget(target).
 *
 * Methods to implement:
 *  - play()
 *  - pause()
 *  - stop()
 *  - seekTo(time)
 *  - load(mediaIndex)
 *  - getMediaDuration()
 *  - getCurrentMediaTime()
 *  - setVolume(volumeSliderPosition)
 *  - mute()
 *  - unMute()
 *  - isMuted()
 *  - updateDisplayMessage()
 */
var PlayerHandler = function(castPlayer) {
    this.target = {};
    this.stream = '';

    this.setTarget= function(target) {
        this.target = target;
    };

    this.setStream = function(stream, mimeType){
        this.stream = stream;
        this.mimeType = mimeType;
    };

    this.play = function() {
        if (castPlayer.playerState !== PLAYER_STATE.PLAYING &&
            castPlayer.playerState !== PLAYER_STATE.PAUSED &&
            castPlayer.playerState !== PLAYER_STATE.LOADED) {
            this.load();
            return;
        }

        this.target.play();
        castPlayer.playerState = PLAYER_STATE.PLAYING;
        document.getElementById('play').style.display = 'none';
        document.getElementById('pause').style.display = 'block';
        this.updateDisplayMessage();
    };

    this.pause = function() {
        if (castPlayer.playerState !== PLAYER_STATE.PLAYING) {
            return;
        }

        this.target.pause();
        castPlayer.playerState = PLAYER_STATE.PAUSED;
        document.getElementById('play').style.display = 'block';
        document.getElementById('pause').style.display = 'none';
        this.updateDisplayMessage();
    };

    this.stop = function() {
        this.pause();
        castPlayer.playerState = PLAYER_STATE.STOPPED;
        this.updateDisplayMessage();
    };

    this.load = function() {
        castPlayer.playerState = PLAYER_STATE.LOADING;
        this.target.load(this.stream, this.mimeType);
        this.updateDisplayMessage();
    };

    this.loaded = function() {
        castPlayer.currentMediaDuration = this.getMediaDuration();
        castPlayer.updateMediaDuration();
        castPlayer.playerState = PLAYER_STATE.LOADED;
        if (castPlayer.currentMediaTime > 0) {
            this.seekTo(castPlayer.currentMediaTime);
        }
        this.play();
        this.updateDisplayMessage();
    };

    this.getCurrentMediaTime = function() {
        return this.target.getCurrentMediaTime();
    };

    this.getMediaDuration = function() {
        return this.target.getMediaDuration();
    };

    this.updateDisplayMessage = function () {
        //this.target.updateDisplayMessage();
    };
};

CastPlayer.prototype.setupRemotePlayer = function () {
    var castSession = cast.framework.CastContext.getInstance().getCurrentSession();

    // Add event listeners for player changes which may occur outside sender app
    this.remotePlayerController.addEventListener(
        cast.framework.RemotePlayerEventType.IS_PAUSED_CHANGED,
        function() {
            if (this.remotePlayer.isPaused) {
                this.playerHandler.pause();
            } else {
                this.playerHandler.play();
            }
        }.bind(this)
    );

    // This object will implement PlayerHandler callbacks with
    // remotePlayerController, and makes necessary UI updates specific
    // to remote playback
    var playerTarget = {};

    playerTarget.play = function () {
        if (this.remotePlayer.isPaused) {
            this.remotePlayerController.playOrPause();
        }

        var vi = document.getElementById('video_image');
        vi.style.display = 'block';
        var localPlayer = document.getElementById('video_element');
        localPlayer.style.display = 'none';
    }.bind(this);

    playerTarget.pause = function () {
        if (!this.remotePlayer.isPaused) {
            this.remotePlayerController.playOrPause();
        }
    }.bind(this);

    playerTarget.stop = function () {
         this.remotePlayerController.stop();
    }.bind(this);

    playerTarget.load = function (stream, mimeType) {
        var mediaInfo = new chrome.cast.media.MediaInfo(
            stream.src, mimeType);

        mediaInfo.metadata = new chrome.cast.media.GenericMediaMetadata();
        mediaInfo.metadata.metadataType = chrome.cast.media.MetadataType.GENERIC;
        mediaInfo.metadata.title = stream.title;
        //mediaInfo.metadata.images = [{'url': stream.preview}];

        var request = new chrome.cast.media.LoadRequest(mediaInfo);
        castSession.loadMedia(request).then(
        this.playerHandler.loaded.bind(this.playerHandler),
        function (errorCode) {
            this.playerState = PLAYER_STATE.ERROR;
            console.log('Remote media load error: ' + errorCode);
        }.bind(this));
    }.bind(this);

    playerTarget.getCurrentMediaTime = function() {
        return this.remotePlayer.currentTime;
    }.bind(this);

    playerTarget.getMediaDuration = function() {
        return this.remotePlayer.duration;
    }.bind(this);

    playerTarget.updateDisplayMessage = function () {
        document.getElementById('playerstate').style.display = 'block';
        document.getElementById('playerstatebg').style.display = 'block';
        document.getElementById('video_image_overlay').style.display = 'block';
        document.getElementById('playerstate').innerHTML = "Cloud Portal Stream";
    }.bind(this);

    this.playerHandler.setTarget(playerTarget);
    this.playerHandler.play();
};
