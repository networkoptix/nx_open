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
package com.networkoptix.hdwitness.ui.widgets;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.PixelFormat;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.common.Resolution;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.internal.PolicyManager;
import com.networkoptix.hdwitness.ui.adapters.ResolutionsListAdapter;

/**
 * A view containing controls for a MediaPlayer. Typically contains the buttons
 * like "Play/Pause", "Rewind" and a progress slider. It takes
 * care of synchronizing the controls with the state of the MediaPlayer.
 * <p>
 * The way to use this class is to instantiate it programatically. The
 * MediaController will create a default set of controls and put them in a
 * window floating above your application. Specifically, the controls will float
 * above the view specified with setAnchorView(). The window will disappear if
 * left idle for three seconds and reappear when the user touches the anchor
 * view.
 * <p>
 */
public class HdwMediaControllerView extends FrameLayout {

    private static final int TIMEOUT_DEFAULT = 5000;
    private static final int FADE_OUT = 1;
    private static final int SHOW_PROGRESS = 2;

    private void Log(String message) {
        if (BuildConfig.DEBUG)
            Log.d(this.getClass().getSimpleName(), message);
    }

    public static abstract class OnVisibilityChangedListener {
        public abstract void onVisibilityChanged(boolean visible);
    }

    private OnVisibilityChangedListener mVisibilityChangedListener = null;

    public void setOnVisibilityChangedListener(OnVisibilityChangedListener listener) {
        mVisibilityChangedListener = listener;
    }

    private static class HdwMediaControllerHandler extends Handler {
        private final WeakReference<HdwMediaControllerView> mApp;

        public HdwMediaControllerHandler(HdwMediaControllerView app) {
            mApp = new WeakReference<HdwMediaControllerView>(app);
        }

        @Override
        public void handleMessage(Message msg) {
            HdwMediaControllerView app = mApp.get();
            if (app == null)
                return;

            switch (msg.what) {
            case FADE_OUT:
                app.hide();
                break;
            case SHOW_PROGRESS:
                app.setProgress();
                if (!app.mDragging && app.mShowing && app.mPlayer != null && app.mPlayer.isPlaying()) {
                    msg = obtainMessage(SHOW_PROGRESS);
                    sendMessageDelayed(msg, 1000);
                }
                break;
            }
        }
    }

    private HdwPlayerControl mPlayer;
    private Context mContext;
    private View mAnchor;
    private View mRoot;
    private WindowManager mWindowManager;
    private Window mWindow;
    private View mDecor;
    private WindowManager.LayoutParams mDecorLayoutParams;
    private SeekBar mProgress;
    private TextView mEndTime, mCurrentTime;
    private boolean mShowing;
    private boolean mDragging;
    private ImageButton mPauseButton;
    private ImageButton mFfwdButton;
    private ImageButton mRewButton;
    private Spinner mResolutionsSpinner;
    private ArrayList<Resolution> mResolutions = new ArrayList<Resolution>();
    private ResolutionsListAdapter mResolutionsAdapter;

    private boolean mNavigationEnabled = true;

    private Handler mHandler = new HdwMediaControllerHandler(this);

    @Override
    public void onFinishInflate() {
        if (mRoot != null)
            initControllerView(mRoot);
    }

    public HdwMediaControllerView(Context context) {
        super(context);
        mContext = context;
        initFloatingWindowLayout();
        initFloatingWindow();
    }

    private void initFloatingWindow() {
        mWindowManager = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);
        mWindow = PolicyManager.makeNewWindow(mContext);
        if (mWindow == null)
            return;
        
        mWindow.setWindowManager(mWindowManager, null, null);
        mWindow.requestFeature(Window.FEATURE_NO_TITLE);
        mDecor = mWindow.getDecorView();
        mDecor.setOnTouchListener(mTouchListener);
        mWindow.setContentView(this);
        mWindow.setBackgroundDrawableResource(android.R.color.transparent);

        // While the media controller is up, the volume control keys should
        // affect the media stream type
        mWindow.setVolumeControlStream(AudioManager.STREAM_MUSIC);

        setFocusable(true);
        setFocusableInTouchMode(true);
        setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        requestFocus();
    }

    // Allocate and initialize the static parts of mDecorLayoutParams. Must
    // also call updateFloatingWindowLayout() to fill in the dynamic parts
    // (y and width) before mDecorLayoutParams can be used.
    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void initFloatingWindowLayout() {
        mDecorLayoutParams = new WindowManager.LayoutParams();
        WindowManager.LayoutParams p = mDecorLayoutParams;
        p.gravity = Gravity.TOP | Gravity.LEFT;
        p.height = LayoutParams.WRAP_CONTENT;
        p.x = 0;
        p.format = PixelFormat.TRANSLUCENT;
        p.type = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;
        p.flags |= WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        if (Utils.hasHoneycomb())
                p.flags |= WindowManager.LayoutParams.FLAG_SPLIT_TOUCH;
        p.token = null;
        p.windowAnimations = 0; // android.R.style.DropDownAnimationDown;
    }

    // Update the dynamic parts of mDecorLayoutParams
    // Must be called with mAnchor != NULL.
    private void updateFloatingWindowLayout() {
        if (mDecor == null)
            return;
        
        int [] anchorPos = new int[2];
        mAnchor.getLocationOnScreen(anchorPos);

        // we need to know the size of the controller so we can properly position it
        // within its space
        mDecor.measure(MeasureSpec.makeMeasureSpec(mAnchor.getWidth(), MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(mAnchor.getHeight(), MeasureSpec.AT_MOST));

        WindowManager.LayoutParams p = mDecorLayoutParams;
        p.width = mAnchor.getWidth();
        p.x = anchorPos[0] + (mAnchor.getWidth() - p.width) / 2;
        p.y = anchorPos[1] + mAnchor.getHeight() - mDecor.getMeasuredHeight();
    }

    private OnTouchListener mTouchListener = new OnTouchListener() {
        public boolean onTouch(View v, MotionEvent event) {
            if (event.getAction() == MotionEvent.ACTION_UP) {
                if (mShowing) {
                    hide();
                }
                return v.performClick();
            }
            return false;
        }
    };
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                show(0); // show until hide is called
                break;
            case MotionEvent.ACTION_UP:
                show(TIMEOUT_DEFAULT); // start timeout
                break;
            case MotionEvent.ACTION_CANCEL:
                hide();
                break;
            default:
                break;
        }
        return performClick();
    }

    public void setMediaPlayer(HdwPlayerControl player) {
        mPlayer = player;
        updatePausePlay();
        updateAllowedResolutions();
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void setAnchorWithListener(View view) {
        
        // This is called whenever mAnchor's layout bound changes
        final OnLayoutChangeListener layoutChangeListener = new OnLayoutChangeListener() {
            public void onLayoutChange(View v, int left, int top, int right,
                    int bottom, int oldLeft, int oldTop, int oldRight,
                    int oldBottom) {
                updateFloatingWindowLayout();
                if (mShowing && mDecor != null) {
                    mWindowManager.updateViewLayout(mDecor, mDecorLayoutParams);
                }
            }
        };

        mAnchor = view;
        if (mAnchor != null) {
            mAnchor.addOnLayoutChangeListener(layoutChangeListener);
        }
    }
    
    /**
     * Set the view that acts as the anchor for the control view. This can for
     * example be a VideoView, or your Activity's main view.
     * 
     * @param view
     *            The view to which to anchor the controller when it is visible.
     */
    public void setAnchorView(View view) {
        if (Utils.hasHoneycomb())
            setAnchorWithListener(view);
        else
            mAnchor = view;

        FrameLayout.LayoutParams frameParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );

        removeAllViews();
        View v = makeControllerView();
        addView(v, frameParams);
    }

    public void setResolution(Resolution r) {
        mPlayer.setResolution(r);
        int idx = mResolutionsAdapter.getPosition(r);
        if (idx < 0)
            return;
        mResolutionsSpinner.setSelection(idx);
    }

    /**
     * Create the view that holds the widgets that control playback. Derived
     * classes can override this to create their own.
     * 
     * @return The controller view.
     * @hide This doesn't work as advertised
     */
    @SuppressLint("InflateParams")
    protected View makeControllerView() {
        LayoutInflater inflate = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mRoot = inflate.inflate(R.layout.media_controller_widget, null);

        initControllerView(mRoot);

        return mRoot;
    }

    private void initControllerView(View v) {
        mPauseButton = (ImageButton) v.findViewById(R.id.pause);
        if (mPauseButton != null) {
            mPauseButton.requestFocus();
            mPauseButton.setOnClickListener(mPauseListener);
        }

        mFfwdButton = (ImageButton) v.findViewById(R.id.ffwd);
        if (mFfwdButton != null) {
            mFfwdButton.setOnClickListener(mFfwdListener);
            mFfwdButton.setVisibility(View.VISIBLE);
        }

        mRewButton = (ImageButton) v.findViewById(R.id.rew);
        if (mRewButton != null) {
            mRewButton.setOnClickListener(mRewListener);
            mRewButton.setVisibility(View.VISIBLE);
        }

        mProgress = (SeekBar) v.findViewById(R.id.mediacontroller_progress);
        if (mProgress != null) {
            mProgress.setOnSeekBarChangeListener(mSeekListener);
            mProgress.setMax(1000);
        }

        mEndTime = (TextView) v.findViewById(R.id.time);
        mCurrentTime = (TextView) v.findViewById(R.id.time_current);

        mResolutionsSpinner = (Spinner) v.findViewById(R.id.spinner_resolution);
        mResolutionsSpinner.setEnabled(false);
        
        mResolutions.clear();
        mResolutions.addAll(Resolution.STANDARD_RESOLUTIONS);
        mResolutionsAdapter = new ResolutionsListAdapter(mContext, mResolutions);
        mResolutionsSpinner.setAdapter(mResolutionsAdapter);
        mResolutionsSpinner.setOnItemSelectedListener(new OnItemSelectedListener() {

            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                if (mPlayer == null)
                    return;
                mPlayer.setResolution(mResolutionsAdapter.getItem(pos));
            }

            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        
        updateAllowedResolutions();
    }

    public void updateAllowedResolutions() {
        if (mPlayer == null || mResolutionsSpinner == null)
            return;

        final List<Resolution> resolutions = mPlayer.allowedResolutions();
        mResolutionsSpinner.setEnabled(resolutions.size() > 1);
        
        if (mResolutions != null && mResolutions.equals(resolutions))
            return;

        Log("updateAllowedResolutions to " + resolutions);
        mResolutionsAdapter.update(resolutions);                     
        mResolutionsSpinner.setSelection(resolutions.indexOf(mPlayer.getResolution()));
    }

    /**
     * Show the controller on screen. It will go away automatically after 3
     * seconds of inactivity.
     */
    public void show() {
        show(TIMEOUT_DEFAULT);
    }

    /**
     * Show the controller on screen. It will go away automatically after
     * 'timeout' milliseconds of inactivity.
     * 
     * @param timeout
     *            The timeout in milliseconds. Use 0 to show the controller
     *            until hide() is called.
     */
    public void show(int timeout) {

        if (!mShowing && mAnchor != null) {
            setProgress();
            if (mPauseButton != null) {
                mPauseButton.requestFocus();
            }

            updateFloatingWindowLayout();
            if (mDecor == null)
                return;
            
            mWindowManager.addView(mDecor, mDecorLayoutParams);
            mShowing = true;
            if (mVisibilityChangedListener != null)
                mVisibilityChangedListener.onVisibilityChanged(mShowing);
        }
        updatePausePlay();

        // cause the progress bar to be updated even if mShowing
        // was already true. This happens, for example, if we're
        // paused with the progress bar showing the user hits play.
        mHandler.removeMessages(SHOW_PROGRESS);
        mHandler.sendEmptyMessage(SHOW_PROGRESS);

        Message msg = mHandler.obtainMessage(FADE_OUT);
        if (timeout != 0) {
            mHandler.removeMessages(FADE_OUT);
            mHandler.sendMessageDelayed(msg, timeout);
        }
    }

    public boolean isShowing() {
        return mShowing;
    }

    /**
     * Remove the controller from the screen.
     */
    public void hide() {
        if (mAnchor == null)
            return;
        if (mShowing && mDecor != null) {
            try {
                mHandler.removeMessages(SHOW_PROGRESS);
                mWindowManager.removeView(mDecor);
            } catch (IllegalArgumentException ex) {
            }
            mShowing = false;
            if (mVisibilityChangedListener != null)
                mVisibilityChangedListener.onVisibilityChanged(mShowing);
        }
    }

    public String stringForTime(int seconds) {
        if (seconds < 0)
            return mContext.getString(R.string.datetime_live_format);
        return DateFormat.format(mContext.getString(R.string.datetime_short_format), 1000L * seconds).toString();
    }

    private void setProgress() {
        if (mPlayer == null || mDragging) {
            return;
        }
        int position = mPlayer.getCurrentPosition();
        int duration = mPlayer.getChunkLength();

        if (duration > 0 && position > mPlayer.getChunkStart() + duration) {
            mPlayer.migrateChunk();
            duration = mPlayer.getChunkLength();
            position = mPlayer.getCurrentPosition();
            updatePausePlay();
        }

        int endValue = -1;
        if (position > 0) {
            if (duration < 0)
                endValue = (int) (System.currentTimeMillis() / 1000);
            else
                endValue = mPlayer.getChunkStart() + duration;
        }

        if (mShowing && mProgress != null) {
            boolean indeterminate = position < 0;
            boolean readyToPlay = mPlayer.isPlaying() || mPlayer.isPreparing();
            mProgress.setIndeterminate(indeterminate && readyToPlay);
            if (!indeterminate) {
                // use long to avoid overflow
                int start = mPlayer.getChunkStart();
                long pos = 1000L * (position - start) / (endValue - start);
                mProgress.setProgress((int) pos);
            }
        }

        if (mShowing && mEndTime != null) {
            if (endValue < 0)
                mEndTime.setText(stringForTime(-1));
            else {
                if (endValue > position)
                    mEndTime.setText(stringForTime(endValue));
                else
                    mEndTime.setText(stringForTime(position));
            }
        }
        if (mShowing && mCurrentTime != null)
            mCurrentTime.setText(stringForTime(position));
    }

    @Override
    public boolean performClick() {
        show(TIMEOUT_DEFAULT);
        return super.performClick();
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        show(TIMEOUT_DEFAULT);
        return false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (event.getRepeatCount() == 0 && event.getAction() == KeyEvent.ACTION_DOWN
                && (keyCode == KeyEvent.KEYCODE_HEADSETHOOK || keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE || keyCode == KeyEvent.KEYCODE_SPACE)) {
            doPauseResume();
            show(TIMEOUT_DEFAULT);
            if (mPauseButton != null) {
                mPauseButton.requestFocus();
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MEDIA_STOP) {
            if (mPlayer != null && mPlayer.isPlaying()) {
                mPlayer.stop();
                updatePausePlay();
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            // don't show the controls for volume adjustment
            return super.dispatchKeyEvent(event);
        } else if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_MENU) {
            hide();

            return true;
        } else {
            show(TIMEOUT_DEFAULT);
        }
        return super.dispatchKeyEvent(event);
    }

    private View.OnClickListener mPauseListener = new View.OnClickListener() {
        public void onClick(View v) {
            doPauseResume();
            show(TIMEOUT_DEFAULT);
        }
    };

    private void updatePausePlay() {
        if (mRoot == null || mPauseButton == null || mPlayer == null)
            return;

        if (mPlayer.isPlaying()) {
            mPauseButton.setImageResource(android.R.drawable.ic_media_pause);
        } else {
            mPauseButton.setImageResource(android.R.drawable.ic_media_play);
        }
    }

    private void doPauseResume() {
        if (mPlayer == null)
            return;

        if (mPlayer.isPlaying()) {
            mPlayer.stop();
        } else {
            mPlayer.start();
        }
        updatePausePlay();
    }

    // There are two scenarios that can trigger the seekbar listener to trigger:
    //
    // The first is the user using the touchpad to adjust the posititon of the
    // seekbar's thumb. In this case onStartTrackingTouch is called followed by
    // a number of onProgressChanged notifications, concluded by
    // onStopTrackingTouch.
    // We're setting the field "mDragging" to true for the duration of the
    // dragging
    // session to avoid jumps in the position in case of ongoing playback.
    //
    // The second scenario involves the user operating the scroll ball, in this
    // case there WON'T BE onStartTrackingTouch/onStopTrackingTouch
    // notifications,
    // we will simply apply the updated position without suspending regular
    // updates.
    private OnSeekBarChangeListener mSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
            show(3600000);

            mDragging = true;

            // By removing these pending progress messages we make sure
            // that a) we won't update the progress while the user adjusts
            // the seekbar and b) once the user is done dragging the thumb
            // we will post one of these messages to the queue again and
            // this ensures that there will be exactly one message queued up.
            mHandler.removeMessages(SHOW_PROGRESS);
        }

        public void onProgressChanged(SeekBar bar, int progress, boolean fromuser) {
            if (!fromuser)
                return;

            if (mPlayer == null)
                return;

            // do not update position on live
            if (mPlayer.getCurrentPosition() < 0)
                return;

            int endValue;
            int duration = mPlayer.getChunkLength();
            if (duration < 0)
                endValue = (int) (System.currentTimeMillis() / 1000);
            else
                endValue = mPlayer.getChunkStart() + duration;
            int startValue = mPlayer.getChunkStart();

            int newPosition = startValue + ((endValue - startValue) * bar.getProgress()) / 1000;

            if (mCurrentTime != null)
                mCurrentTime.setText(stringForTime(newPosition));
        }

        public void onStopTrackingTouch(SeekBar bar) {
            mDragging = false;

            if (mPlayer == null)
                return;

            // do not update position on live
            if (mPlayer.getCurrentPosition() < 0)
                return;

            int endValue;
            int duration = mPlayer.getChunkLength();
            if (duration < 0)
                endValue = (int) (System.currentTimeMillis() / 1000);
            else
                endValue = mPlayer.getChunkStart() + duration;
            int startValue = mPlayer.getChunkStart();

            int newPosition = startValue + ((endValue - startValue) * bar.getProgress()) / 1000;
            mPlayer.setCurrentPosition(newPosition);

            setProgress();
            updatePausePlay();
            show(TIMEOUT_DEFAULT);

            // Ensure that progress is properly updated in the future,
            // the call to show() does not guarantee this because it is a
            // no-op if we are already showing.
            mHandler.sendEmptyMessage(SHOW_PROGRESS);
        }
    };

    @Override
    public void setEnabled(boolean enabled) {
        if (mPauseButton != null) {
            mPauseButton.setEnabled(enabled);
        }
        updateNavigationControls(enabled & mNavigationEnabled);
        super.setEnabled(enabled);
    }

    public void setNavigationEnabled(boolean enabled) {
        if (mNavigationEnabled == enabled)
            return;
        mNavigationEnabled = enabled;
        updateNavigationControls(mNavigationEnabled & isEnabled());

    }

    public void updateNavigationControls(boolean enabled) {
        if (mFfwdButton != null) {
            mFfwdButton.setEnabled(enabled);
        }
        if (mRewButton != null) {
            mRewButton.setEnabled(enabled);
        }
        if (mProgress != null) {
            mProgress.setEnabled(enabled);
        }
    }

    private View.OnClickListener mRewListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (!isEnabled() || mPlayer == null)
                return;

            mPlayer.previousChunk();
            setProgress();
            show(TIMEOUT_DEFAULT);
        }
    };

    private View.OnClickListener mFfwdListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (!isEnabled() || mPlayer == null)
                return;

            mPlayer.nextChunk();
            setProgress();
            show(TIMEOUT_DEFAULT);
        }
    };

    public interface HdwPlayerControl {
        boolean isPlaying();

        /**
         * Update chunks limits for the currently playing chunk. Called when new
         * chunks are loaded.
         */
        void updateCurrentChunk();

        /**
         * Soft switch to the next chunk. Called when previous chunk has ended.
         */
        void migrateChunk();

        void start();

        void stop();

        /**
         * Get start time of the current chunk.
         * 
         * @return int value in secs since epoch.
         */
        int getChunkStart();

        /**
         * Get current playing position.
         * 
         * @return int value in secs since epoch. Value <b>-1</b> means live
         *         video.
         */
        int getCurrentPosition();

        /**
         * Set current playing position.
         * 
         * @param msecsSinceEpoch
         *            - int value in secs since epoch. -1 means live video.
         */
        void setCurrentPosition(int secsSinceEpoch);

        int getChunkLength();

        void nextChunk();

        void previousChunk();

        boolean isPreparing();

        Resolution getResolution();

        void setResolution(Resolution resolution);

        List<Resolution> allowedResolutions();
    }

}
