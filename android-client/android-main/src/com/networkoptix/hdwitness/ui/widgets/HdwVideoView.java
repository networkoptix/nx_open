package com.networkoptix.hdwitness.ui.widgets;

/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnErrorListener;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.text.format.DateFormat;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.Chunk;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.common.HttpHeaders;
import com.networkoptix.hdwitness.common.MediaStreams;
import com.networkoptix.hdwitness.common.MediaStreams.MediaTransport;
import com.networkoptix.hdwitness.common.Resolution;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.ui.widgets.HdwMediaControllerView.HdwPlayerControl;

/**
 * Displays a video file. The VideoView class can load images from various
 * sources (such as resources or content providers), takes care of computing its
 * measurement from the video so that it can be used in any layout manager, and
 * provides various display options such as scaling and tinting.
 */
public class HdwVideoView extends SurfaceView implements HdwPlayerControl {

    public interface OnResolutionChangedListener {
        void resolutionChanged();
    }

    @SuppressWarnings("serial")
    private static final Set<MediaTransport> TRANSPORTS = new HashSet<MediaTransport>() {
        {
            add(MediaTransport.RTSP);
            if (Utils.hasIcecream())
                add(MediaTransport.HTTP);
        }
    };

    public interface SourceParametersProvider {
        String getVideoUrl(MediaTransport transport, Resolution resolution, long positionMs);
        double getAspectRatio();
    }

    // settable by the client
    private Uri mUri;
    private Context mContext;

    // all possible internal states
    private static final int STATE_ERROR = -1;
    private static final int STATE_IDLE = 0;
    private static final int STATE_PREPARING = 1;
    private static final int STATE_PREPARED = 2;
    private static final int STATE_PLAYING = 3;

    // mCurrentState is a VideoView object's current state.
    // mTargetState is the state that a method caller intends to reach.
    // For instance, regardless the VideoView object's current state,
    // calling pause() intends to bring the object to a target state
    // of STATE_PAUSED.
    private int mCurrentState = STATE_IDLE;
    private int mTargetState = STATE_IDLE;

    // All the stuff we need for playing and showing a video
    private SurfaceHolder mSurfaceHolder = null;
    private MediaPlayer mMediaPlayer = null;
    private int mVideoWidth;
    private int mVideoHeight;
    private int mSurfaceWidth;
    private int mSurfaceHeight;
    private Resolution mVideoResolution = null;
    private HdwMediaControllerView mMediaController;
    private MediaPlayer.OnPreparedListener mOnPreparedListener;
    private OnErrorListener mOnErrorListener;
    private SourceParametersProvider mSourceParametersProvider;

    /**
     * Position from which the playing was started, in seconds. Value -1 means
     * live video.
     */
    private int mPlayPos = -1;

    /**
     * Value that will be subtracted from current media player progress when
     * making transition from one chunk to another.
     */
    private int mChunkTransitionBuffer = 0;

    /**
     * View that will be displayed over the video if camera is offline and user
     * is trying to play live video.
     */
    private View mOfflineLabel;

    /**
     * Is the camera offline at the moment.
     */
    private boolean mIsOffline;

    private float mScaleFactor = 1.0f;

    /**
     * Time when the player really started to show video.
     */
    private int mStartAt;

    private ChunkList mChunks;
    private Chunk mCurrentChunk;

    private MediaTransport mTransport = MediaTransport.getDefault();
    
    /** All possible resolutions for camera. */
    private Map<MediaTransport, List<Resolution>> mCameraResolutions;

    private OnResolutionChangedListener mOnResolutionChangedListener;

    private MediaStreams mMediaStreams;
    private HttpHeaders mCustomHeaders = new HttpHeaders();

    private static class ResizeHandler extends Handler {

        private final WeakReference<HdwVideoView> mOwner;

        public ResizeHandler(HdwVideoView owner) {
            mOwner = new WeakReference<HdwVideoView>(owner);
        }

        @Override
        public void handleMessage(Message msg) {
            HdwVideoView owner = mOwner.get();
            if (owner == null)
                return;
            if (owner.mTargetState == STATE_PLAYING)
                owner.setPlaying(true);
        }
    }

    private final Handler mHandler = new ResizeHandler(this);

    public HdwVideoView(Context context) {
        super(context);
        mContext = context;
        initVideoView();
    }

    public HdwVideoView(Context context, AttributeSet attrs) {
        super(context, attrs, 0);
        mContext = context;
        initVideoView();
    }

    public HdwVideoView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mContext = context;
        initVideoView();
    }

    private String debugTime(int secondsSinceEpoch) {
        if (secondsSinceEpoch < 0)
            return mContext.getString(R.string.datetime_live_format);
        return DateFormat.format(mContext.getString(R.string.datetime_short_format), 1000L * secondsSinceEpoch).toString();
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = getDefaultSize(mVideoWidth, widthMeasureSpec);
        int height = getDefaultSize(mVideoHeight, heightMeasureSpec);
        if (mVideoWidth > 0 && mVideoHeight > 0) {
            if (mVideoWidth * height > width * mVideoHeight) {
                height = width * mVideoHeight / mVideoWidth;
            } else if (mVideoWidth * height < width * mVideoHeight) {
                width = height * mVideoWidth / mVideoHeight;
            }
        }

        if (mVideoResolution == null && mTargetState == STATE_PLAYING) {
            mHandler.sendMessageDelayed(mHandler.obtainMessage(), 1);
            setResolution(bestResolution(width, height));
        }

        setMeasuredDimension(width, height);
    }

    @SuppressWarnings("deprecation")
    private void initVideoView() {
        mVideoWidth = 0;
        mVideoHeight = 0;
        getHolder().addCallback(mSHCallback);
        // deprecated setting, but required on Android versions prior to 3.0. DO
        // NOT REMOVE!!!
        getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setKeepScreenOn(true);
        requestFocus();
        mCurrentState = STATE_IDLE;
        mTargetState = STATE_IDLE;
    }

    public void setVideoPath(String path) {
        Uri targetUri = Uri.parse(path);
        
        if (mUri != null && mUri.compareTo(targetUri) == 0) {
            boolean isAlreadyPlaying = mCurrentState == STATE_PREPARING || mCurrentState == STATE_PREPARED || mCurrentState == STATE_PLAYING; 
            if (mTargetState == STATE_PLAYING && isAlreadyPlaying)
                return;
            Log("set the same uri while state is " + mCurrentState + " and target state is " + mTargetState);
        }

        Log("opening url " + path);
        mUri = targetUri;
        openVideo();
        requestLayout();
        invalidate();
    }

    private void stopPlayback() {
        if (mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.release();
            mMediaPlayer = null;
            mCurrentState = STATE_IDLE;
            mTargetState = STATE_IDLE;
        }
    }
    
    private void storePosition(int position) {
        Log("Store position " + debugTime(position));
        mPlayPos = position;
    }

    @TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH)
    private void setDataSource() {
        Map<String, String> headers = mCustomHeaders.toMap();
        
        if (Utils.hasIcecream()) {
            try {
                mMediaPlayer.setDataSource(mContext, mUri, headers);
                return;
            } catch (IllegalArgumentException e) {
                e.printStackTrace();
            } catch (SecurityException e) {
                e.printStackTrace();
            } catch (IllegalStateException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        Method method;
        try {
            method = mMediaPlayer.getClass().getMethod("setDataSource", new Class[] { Context.class, Uri.class, Map.class });
            method.invoke(mMediaPlayer, new Object[] { mContext, mUri, headers });
            return;
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }

        try {
            mMediaPlayer.setDataSource(mUri.toString());
            return;
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        } catch (SecurityException e) {
            e.printStackTrace();
        } catch (IllegalStateException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }

        mErrorListener.onError(mMediaPlayer, MediaPlayer.MEDIA_ERROR_UNKNOWN, 0);
    }

    private void openVideo() {
        if (mUri == null || mSurfaceHolder == null) {
            // not ready for playback just yet, will try again later
            return;
        }
        // Tell the music playback service to pause
        Intent i = new Intent("com.android.music.musicservicecommand");
        i.putExtra("command", "pause");
        mContext.sendBroadcast(i);

        // we shouldn't clear the target state, because somebody might have
        // called start() previously
        release(false);
        try {

            Log("openVideo");
            mMediaPlayer = new MediaPlayer();
            mMediaPlayer.reset();
            mMediaPlayer.setOnPreparedListener(mPreparedListener);
            mMediaPlayer.setOnVideoSizeChangedListener(mSizeChangedListener);
            mMediaPlayer.setOnErrorListener(mErrorListener);
            setDataSource();
            mMediaPlayer.setDisplay(mSurfaceHolder);
            mMediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);
            mMediaPlayer.setScreenOnWhilePlaying(true);
            mMediaPlayer.prepareAsync();
            // we don't set the target state here either, but preserve the
            // target state that was there before.
            mCurrentState = STATE_PREPARING;

            Log("openVideo: preparing...");
        } catch (IllegalArgumentException ex) {

            Log("openVideo: Illegal Argument Exception");
            mCurrentState = STATE_ERROR;
            mTargetState = STATE_ERROR;
            mErrorListener.onError(mMediaPlayer, MediaPlayer.MEDIA_ERROR_UNKNOWN, 0);
            return;
        }
    }

    public void setMediaController(HdwMediaControllerView controller) {
        if (mMediaController != null) {
            mMediaController.hide();
            mMediaController.setEnabled(false);
            mMediaController.setMediaPlayer(null);
        }
        mMediaController = controller;
        if (mMediaController != null) {
            mMediaController.setMediaPlayer(this);
            mMediaController.setEnabled(true );
        }
    }

    MediaPlayer.OnVideoSizeChangedListener mSizeChangedListener = new MediaPlayer.OnVideoSizeChangedListener() {
        public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
            mVideoWidth = mp.getVideoWidth();
            mVideoHeight = mp.getVideoHeight();
            if (mVideoWidth != 0 && mVideoHeight != 0) {
                getHolder().setFixedSize((int) (mVideoWidth * mScaleFactor), (int) (mVideoHeight * mScaleFactor));
            }
        }
    };

    MediaPlayer.OnPreparedListener mPreparedListener = new MediaPlayer.OnPreparedListener() {
        public void onPrepared(MediaPlayer mp) {
            mCurrentState = STATE_PREPARED;

            if (mOnPreparedListener != null) {
                mOnPreparedListener.onPrepared(mMediaPlayer);
            }
            mVideoWidth = mp.getVideoWidth();
            mVideoHeight = mp.getVideoHeight();

            if (mVideoWidth != 0 && mVideoHeight != 0) {
                getHolder().setFixedSize((int) (mVideoWidth * mScaleFactor), (int) (mVideoHeight * mScaleFactor));
                if (mSurfaceWidth == (int) (mVideoWidth * mScaleFactor) && mSurfaceHeight == (int) (mVideoHeight * mScaleFactor)) {
                    // We didn't actually change the size (it was already at the
                    // size we need), so
                    // we won't get a "surface changed" callback, so start the
                    // video here instead
                    // of in the callback.
                    if (mTargetState == STATE_PLAYING) {
                        startMediaPlayer();
                        if (mMediaController != null) {
                            mMediaController.show();
                        }
                    } else if (!isPlaying() && (getCurrentPosition() > 0)) {
                        if (mMediaController != null) {
                            // Show the media controls when we're paused into a
                            // video and make'em stick.
                            mMediaController.show(0);
                        }
                    }
                }
            } else {
                // We don't know the video size yet, but should start anyway.
                // The video size might be reported to us later.
                if (mTargetState == STATE_PLAYING) {
                    startMediaPlayer();
                }
            }
        }
    };

    private MediaPlayer.OnErrorListener mErrorListener = new MediaPlayer.OnErrorListener() {
        public boolean onError(MediaPlayer mp, int framework_err, int impl_err) {

            Log("mp error: " + framework_err + " / " + impl_err);

            mCurrentState = STATE_ERROR;
            mTargetState = STATE_ERROR;
            if (mMediaController != null) {
                mMediaController.hide();
            }

            /*
             * If an error handler has been supplied , use it and finish .
             */
            if (mOnErrorListener != null) {
                mOnErrorListener.onError(mMediaPlayer, framework_err, impl_err);
            }
            return true;
        }
    };

     /**
     * Register a callback to be invoked when the media file is loaded and ready
     * to go.
     * 
     * @param l
     *            The callback that will be run
     */
    public void setOnPreparedListener(MediaPlayer.OnPreparedListener l) {
        mOnPreparedListener = l;
    }

    /**
     * Register a callback to be invoked when an error occurs during playback or
     * setup. If no listener is specified, or if the listener returned false,
     * VideoView will inform the user of any errors.
     * 
     * @param l
     *            The callback that will be run
     */
    public void setOnErrorListener(OnErrorListener l) {
        mOnErrorListener = l;
    }

    SurfaceHolder.Callback mSHCallback = new SurfaceHolder.Callback() {
        public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
            mSurfaceWidth = w;
            mSurfaceHeight = h;
            boolean isValidState = (mTargetState == STATE_PLAYING);
            boolean hasValidSize = (mVideoWidth * mScaleFactor == w && mVideoHeight * mScaleFactor == h);
            if (mMediaPlayer != null && isValidState && hasValidSize) {
                startMediaPlayer();
                if (mMediaController != null) {
                    if (mMediaController.isShowing()) {
                        // ensure the controller will get repositioned later
                        mMediaController.hide();
                    }
                }
            }
        }

        public void surfaceCreated(SurfaceHolder holder) {
            mSurfaceHolder = holder;
            openVideo();
        }

        public void surfaceDestroyed(SurfaceHolder holder) {
            // after we return from this we can't use the surface any more
            mSurfaceHolder = null;
            if (mMediaController != null)
                mMediaController.hide();
            release(true);
        }
    };



    /**
     * Release the media player in any state
     * 
     * @param clearTargetState
     *            - if set to True, mTargetState will be set to IDLE
     */
    private void release(boolean clearTargetState) {
        if (mMediaPlayer != null) {
            // save playback position
            long offset = (mCurrentState == STATE_PLAYING) ? (System.currentTimeMillis() / 1000 - mStartAt) : 0;
            int smallOffset = (int) offset;
            if (BuildConfig.DEBUG && smallOffset != offset)
                throw new AssertionError("Offset " + offset + " is too big");
            
            storePosition(mPlayPos + smallOffset - mChunkTransitionBuffer);
            mChunkTransitionBuffer = 0;

            mMediaPlayer.reset();
            mMediaPlayer.release();
            mMediaPlayer = null;
            mCurrentState = STATE_IDLE;
            if (clearTargetState) {
                mTargetState = STATE_IDLE;
            }
        }
    }

    @Override
    public boolean performClick() {
        toggleMediaControlsVisiblity();
        return super.performClick();
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        toggleMediaControlsVisiblity();
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isKeyCodeSupported = keyCode != KeyEvent.KEYCODE_BACK && keyCode != KeyEvent.KEYCODE_VOLUME_UP
                && keyCode != KeyEvent.KEYCODE_VOLUME_DOWN && keyCode != KeyEvent.KEYCODE_MENU && keyCode != KeyEvent.KEYCODE_CALL
                && keyCode != KeyEvent.KEYCODE_ENDCALL;
        if (isInPlaybackState() && isKeyCodeSupported && mMediaController != null) {
            if (keyCode == KeyEvent.KEYCODE_HEADSETHOOK || keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE) {
                if (mMediaPlayer.isPlaying()) {
                    stop();
                    mMediaController.show();
                } else {
                    startMediaPlayer();
                    mMediaController.hide();
                }
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MEDIA_STOP && mMediaPlayer.isPlaying()) {
                stop();
                mMediaController.show();
            } else {
                toggleMediaControlsVisiblity();
            }
        }

        return super.onKeyDown(keyCode, event);
    }

    private void toggleMediaControlsVisiblity() {
        if (mMediaController == null)
            return;

        if (mMediaController.isShowing()) {
            mMediaController.hide();
        } else {
            mMediaController.show();
        }
    }

    private void startMediaPlayer() {
        if (isInPlaybackState()) {
            mMediaPlayer.start();            
            if (mCurrentState != STATE_PLAYING) {
                Log("Start playing at " + debugTime((int) (System.currentTimeMillis() / 1000)));
                mStartAt = (int) (System.currentTimeMillis() / 1000);
                mCurrentState = STATE_PLAYING;
            }
        }
        mTargetState = STATE_PLAYING;
    }

    public void startLive() {
        Log("startLive");
        storePosition(-1);
        setPlaying(true);
    }

    public void startArchive(int pos) {
        Log("startArchive " + debugTime(pos));
        storePosition(mPlayPos);
        setPlaying(true);
    }

    private int checkPlayPos() {
        if (mCurrentChunk == null)
            return -1;

        if (mPlayPos < mCurrentChunk.start)
            return mCurrentChunk.start;

        return mPlayPos;
    }

    private void setPlaying(boolean playing) {
        mCurrentChunk = (mChunks == null) || (mPlayPos < 0) ? null : mChunks.nearest(mPlayPos);
        storePosition(checkPlayPos());
        Log("setPlaying " + playing + " at " + debugTime(mPlayPos) + " chunk " + mCurrentChunk + " : " + mTransport + " : " + mVideoResolution);

        mChunkTransitionBuffer = 0;
        mTargetState = STATE_IDLE;

        if (isPlaying())
            stopPlayback();

        updateOfflineLabel();

        if (!playing)
            return;

        if (mPlayPos < 0 && mIsOffline)
            return;

        mTargetState = STATE_PLAYING;

        // check if have not initialized yet
        if (mVideoResolution == null)
            return;

        if (mSourceParametersProvider == null)
            return;

        String videoUrl = mSourceParametersProvider.getVideoUrl(mTransport, mVideoResolution, (long)mPlayPos * 1000);

        if (videoUrl == null)
            return;

        setVideoPath(videoUrl);

        requestFocus();
    }

    public int getCurrentPosition() {
        if (mCurrentChunk == null)
            return -1;

        if (mMediaPlayer == null)
            return mPlayPos;

        int offset = (mCurrentState == STATE_PLAYING) ? ((int) (System.currentTimeMillis() / 1000) - mStartAt) : 0;
        
        return mPlayPos + offset - mChunkTransitionBuffer;
    }

    public void setCurrentPosition(int secsSinceEpoch) {
        Log("setCurrentPosition " + debugTime(secsSinceEpoch));
        storePosition(secsSinceEpoch);
        if (isPlaying())
            start();
    }

    public boolean isPlaying() {
        return (isInPlaybackState() && mMediaPlayer.isPlaying()) || mTargetState == STATE_PLAYING;
    }

    private boolean isInPlaybackState() {
        return (mMediaPlayer != null && mCurrentState != STATE_ERROR && mCurrentState != STATE_IDLE && mCurrentState != STATE_PREPARING);
    }

    public boolean canPause() {
        return true;
    }

    private Chunk getNextChunk() {
        return (mChunks != null) ? mChunks.nextOf(mCurrentChunk) : null;
    }

    private Chunk getPreviousChunk() {
        return (mChunks != null) ? mChunks.previousOf(mCurrentChunk) : null;
    }

    public boolean canSeekBackward() {
        Chunk prev = getPreviousChunk();
        return prev != null && prev != mCurrentChunk;
    }

    public boolean canSeekForward() {
        Chunk next = getNextChunk();
        return next != null && next != mCurrentChunk;
    }

    public void nextChunk() {
        Log("nextChunk current " + debugTime(mPlayPos) + " " + mCurrentChunk);
        mCurrentChunk = getNextChunk();
        if (mCurrentChunk != null)
            storePosition(mCurrentChunk.start);
        else
            storePosition(-1);
        Log("nextChunk setTo " + debugTime(mPlayPos) + " " + mCurrentChunk);
        setPlaying(isPlaying());
    }

    public void previousChunk() {
        Log("prevChunk current " + debugTime(mPlayPos) + " " + mCurrentChunk);
        mCurrentChunk = getPreviousChunk();
        if (mCurrentChunk != null)
            storePosition(mCurrentChunk.start);
        else
            storePosition(-1);
        Log("prevChunk setTo " + debugTime(mPlayPos) + " " + mCurrentChunk);
        setPlaying(isPlaying());
    }

    public void setChunks(ChunkList chunks) {
        mChunks = chunks;
    }

    public int getChunkStart() {
        if (mCurrentChunk == null)
            return -1;
        return mCurrentChunk.start;
    }

    public int getChunkLength() {
        if (mCurrentChunk == null)
            return -1;
        return mCurrentChunk.end - mCurrentChunk.start;
    }

    public void start() {
        setPlaying(true);
    }

    public void stop() {
        mPlayPos = getCurrentPosition();
        
        setPlaying(false);
        stopPlayback();
    }

    public void migrateChunk() {
        if (!canSeekForward())
            return;
        Log("migrateChunk current " + debugTime(mPlayPos) + " " + mCurrentChunk);
        mChunkTransitionBuffer = (int) (System.currentTimeMillis() / 1000) - mStartAt;
        mCurrentChunk = getNextChunk();
        storePosition(mCurrentChunk != null ? mCurrentChunk.start : -1);
        Log("migrateChunk setTo " + debugTime(mPlayPos) + " " + mCurrentChunk);
        if (mIsOffline && mPlayPos < 0)
            setPlaying(false);
    }

    public void updateCurrentChunk() {
        Chunk updated = (mChunks == null) || (mPlayPos < 0) ? null : mChunks.nearest(mPlayPos);
        if (updated == null)
            return;

        if (mCurrentChunk != null && mCurrentChunk.start == updated.start)
            mCurrentChunk = updated;
    }

    /**
     * Setup view that will be displayed if camera is offline.
     * 
     * @param v
     *            - provided view or null.
     */
    public void setOfflineLabel(View v) {
        if (mOfflineLabel != null)
            mOfflineLabel.setVisibility(View.GONE);
        mOfflineLabel = v;
        updateOfflineLabel();
    }

    private void updateOfflineLabel() {
        if (mOfflineLabel == null)
            return;

        if (mIsOffline && mPlayPos < 0)
            mOfflineLabel.setVisibility(View.VISIBLE);
        else
            mOfflineLabel.setVisibility(View.GONE);
    }

    public boolean isLiveAllowed() {
        if (mIsOffline)
            return false;
        return (!isPlaying() || mPlayPos >= 0);
    }

    public void setIsOffline(boolean offline) {
        if (mIsOffline == offline)
            return;
        mIsOffline = offline;
        if (mPlayPos < 0)
            setPlaying(!offline);
    }

    public boolean isOffline() {
        return mIsOffline;
    }

    public boolean isPreparing() {
        return mCurrentState == STATE_PREPARING;
    }

    public Resolution getResolution() {
        return mVideoResolution;
    }

    public void setResolution(Resolution resolution) {
        if (resolution == null)
            return;
        
        if (resolution.equals(mVideoResolution))
            return;

        Log("set resolution to " + resolution);
        mVideoResolution = resolution;
        
        if (isPlaying()) {
            Log("Restarting playing video");
            stop();
            start();
        }

        if (mOnResolutionChangedListener != null)
            mOnResolutionChangedListener.resolutionChanged();
    }

    public Map<MediaTransport, List<Resolution>> getCameraResolutions() {
        return mCameraResolutions;
    }

    public List<Resolution> allowedResolutions() {
        ArrayList<Resolution> allowedResolutions = mMediaStreams != null ? mMediaStreams.allowedResolutions(mTransport) : new ArrayList<Resolution>();

        if (allowedResolutions.isEmpty())
            allowedResolutions.addAll(Resolution.STANDARD_RESOLUTIONS);
        return allowedResolutions;
    }

    public Set<MediaTransport> allowedTransports() {
        HashSet<MediaTransport> allowedTransports = mMediaStreams != null ? mMediaStreams.allowedTransports() : new HashSet<MediaTransport>();

        if (allowedTransports.isEmpty())
            allowedTransports.addAll(TRANSPORTS);
        return allowedTransports;
    }

    private Resolution bestResolution(int width, int height) {
        if (height <= 0)
            return null;
        
        Log("calculating best resolution to width " + width +  " height " + height + " from " + allowedResolutions().toString());
        
        /* Defaulting to max standard aspect ratio. */
        double aspectRatio = mSourceParametersProvider != null
                ? mSourceParametersProvider.getAspectRatio()
                : 16.0 / 9.0;        
       
        
        int nearestDifference = -1;
        int nearestWidthOverflow = -1;
        
        Resolution nearest = null;
        for (Resolution r : allowedResolutions()) {
            /* Native resolution is never the best as it can be not in the baseline-profile. */
            if (r.isNative() && nearest != null)
                continue;
            
            int calculatedWidth = r.getWidth() <= 0 
                    ? (int)(r.getHeight() * aspectRatio)
                    : r.getWidth();
            int widthOverflow = Math.max(calculatedWidth - width, 0);

            /* Width overflow must be minimal. */
            if (nearest != null
                    && !nearest.isNative()
                    && nearestWidthOverflow < widthOverflow)
                continue;
                
            int difference = Math.abs(r.getHeight() - height);
            if (nearest != null 
                    && difference >= nearestDifference 
                    && !nearest.isNative())
                continue;

            nearestDifference = difference;
            nearestWidthOverflow = widthOverflow;
            nearest = r;
        }
        return nearest;
    }

    private void resetResolution() {
        if (mVideoResolution != null)
            setResolution(bestResolution(mSurfaceWidth, mVideoResolution.getHeight()));
        else
            setResolution(bestResolution(mSurfaceWidth, mVideoHeight));     
    }
    
    public void setTransport(MediaTransport transport) {
        Log("try to set transport to " + transport);
        if (mTransport == transport)
            return;

        if (!allowedTransports().contains(transport))
            return;

        Log("transport successfully set to " + transport);

        boolean wasPlaying = isPlaying();
        if (wasPlaying)
            stop();
        
        mTransport = transport;
        resetResolution();
        
        if (wasPlaying)
            start();
    }

    public MediaTransport getTransport() {
        return mTransport;
    }

    public String getUri() {
        return mUri.toString();
    }

    public SourceParametersProvider getVideoUrlResolver() {
        return mSourceParametersProvider;
    }

    public void setVideoUrlResolver(SourceParametersProvider videoUrlResolver) {
        mSourceParametersProvider = videoUrlResolver;
    }

    public void setOnResolutionChangedListener(OnResolutionChangedListener onResolutionChangedListener) {
        mOnResolutionChangedListener = onResolutionChangedListener;
    }

    public void setMediaStreams(MediaStreams mediaStreams) {
        mMediaStreams = mediaStreams;
        
        Set<MediaTransport> allowedTransports = allowedTransports();
        if (!allowedTransports.contains(getTransport()))
            setTransport(MediaTransport.getDefault(allowedTransports));
        else 
            resetResolution();
    }

    public MediaStreams getMediaStreams() {
        return mMediaStreams;
    }

    public void setCustomHeaders(HttpHeaders customHeaders) {
        mCustomHeaders = customHeaders;        
    }

}
