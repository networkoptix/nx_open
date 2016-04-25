package com.networkoptix.hdwitness.ui.activities;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map.Entry;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Set;
import java.util.UUID;

import android.annotation.TargetApi;
import android.app.ActionBar;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.ActivityInfo;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.support.v4.content.LocalBroadcastManager;
import android.text.format.DateFormat;
import android.util.Base64;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.gson.Gson;
import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.api.resources.CameraResource;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.UserResource;
import com.networkoptix.hdwitness.common.MediaStreams;
import com.networkoptix.hdwitness.common.MediaStreams.MediaTransport;
import com.networkoptix.hdwitness.common.Resolution;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.common.network.Constants;
import com.networkoptix.hdwitness.common.network.StreamLoadingTask;
import com.networkoptix.hdwitness.ui.HdwApplication;
import com.networkoptix.hdwitness.ui.UiConsts;
import com.networkoptix.hdwitness.ui.widgets.HdwMediaControllerView;
import com.networkoptix.hdwitness.ui.widgets.HdwVideoView;

//TODO: change surface to OpenGL-based, use pixel shaders:
// http://stackoverflow.com/questions/4850025/shader-for-android-opengl-es
// http://blog.shayanjaved.com/2011/03/13/shaders-android/
// http://stackoverflow.com/questions/9375598/android-how-play-video-on-surfaceopengl
public class VideoActivity extends HdwActivity implements HdwVideoView.SourceParametersProvider {

    private final class State {
        public int Position;
        public boolean Playing;
        public boolean Offline;
        public boolean Preparing;
        public boolean LiveAllowed;

        public State(HdwVideoView view) {
            Position = view.getCurrentPosition();
            Playing = view.isPlaying();
            Offline = view.isOffline();
            Preparing = view.isPreparing();
            LiveAllowed = view.isLiveAllowed();
        }

        @Override
        public boolean equals(Object o) {
            if (o == null)
                return false;

            State another = (o instanceof State) ? (State) o : null;
            if (another == null)
                return false;

            return this.Position == another.Position && this.Playing == another.Playing && this.Offline == another.Offline
                    && this.Preparing == another.Preparing && this.LiveAllowed == another.LiveAllowed;
        }
    }

    private static final int MESSAGE_PROGRESS               = 0;
    private static final int MESSAGE_ACTION_BAR_FADE_OUT    = 1;

    private static final long ACTION_BAR_DEFAULT_TIMEOUT    = 10000;
    private static final long ACTION_BAR_UNPIN_TIMEOUT      = 1500;
    private static final long CHUNKS_ERROR_RETRY_TIMEOUT    = 15000;

    private static final String MEDIA_STREAMS_PARAM         = "mediaStreams";
    private static final String FORCED_AR_PARAM             = "overrideAr";
    private static final String VIDEO_LAYOUT_PARAM          = "VideoLayout";

    private static final String STORED_PLAY_POS             = "play_pos";
    private static final String STORED_CHUNKS               = "chunks";
    private static final String STORED_SUCCESS_TIME         = "chunks_success";

    private static final String ENTRY_PREFERRED_MODE        = "preferred_mode";
    private static final String ENTRY_ACTION_BAR_PINNED     = "action_bar_pinned";

    public static final String CAMERA_ID_UUID               = "camera_id";

    private static final int NO_MODE                        = -1;
    
    private static final double DEFAULT_VIDEO_ASPECT_RATIO  = 16.0 / 9.0;
    
    /* Layout example: "width=4;height=1;sensors=2,1,3,0"  */
    private static final Pattern mVideoLayoutPattern = Pattern.compile("width=(\\d+);height=(\\d+).*");

    private State mState;
    private boolean mArchiveEnabled = false;
    private boolean mActionBarPinned = false;
    private int mPreferredMode = NO_MODE;

    private HdwMediaControllerView mMediaController;
    private HdwVideoView mVideoView;
    private TextView mDetailsView;

    private ImageView mLiveButton;
    private ImageView mArchiveButton;

    @SuppressWarnings("serial")
    private HashMap<MediaTransport, Integer> mTransportMenuItems = new HashMap<MediaTransport, Integer>() {
        {
            put(MediaTransport.RTSP, R.id.menu_transport_rtsp);
            if (Utils.hasIcecream())
                put(MediaTransport.HTTP, R.id.menu_transport_http);

        }
    };

    /**
     * Position from which the playing was started, in seconds. Value -1 means
     * live video.
     */
    private int mPlayPos = -1;
    private boolean mUsingProxy;

    private long mChunksSuccessTime = 0;
    private long mChunksErrorTime = 0;

    private boolean mTransportSwitched = false;

    private ChunkList mChunks = new ChunkList();

    private List<ChunksLoadingTask> mChunksLoadingTasks = new ArrayList<ChunksLoadingTask>();

    private BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(UiConsts.BCAST_LOGOFF)) {
                finish();
            } else if (intent.getAction().equals(UiConsts.BCAST_RESOURCES_UPDATED)) {
                updateCameraResource();
            }
        }
    };

    private UUID mCameraId;
    private CameraResource mCamera;
    private String mEncodedCameraPhysicalId;

    @Override
    protected void handleMessage(Message msg) {
        switch (msg.what) {
        case MESSAGE_PROGRESS:
            updateProgress();
            msg = mHandler.obtainMessage(MESSAGE_PROGRESS);
            mHandler.sendMessageDelayed(msg, 1000);
            break;
        case MESSAGE_ACTION_BAR_FADE_OUT:
            if (Utils.hasHoneycomb())
                hideActionBar();
            mDetailsView.setVisibility(View.VISIBLE);
            break;
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void hideActionBar() {
        ActionBar actionBar = getActionBar();
        if (actionBar != null)
            actionBar.hide();
    }

    private String debugTime(int secondsSinceEpoch) {
        if (secondsSinceEpoch < 0)
            return getString(R.string.datetime_live_format);
        return DateFormat.format(getString(R.string.datetime_short_format), 1000L * secondsSinceEpoch).toString();
    }

    @Override
    protected void migratePreferences(int oldVersion, SharedPreferences oldPreferences, Editor editor) {
        super.migratePreferences(oldVersion, oldPreferences, editor);

        if (oldVersion == 0) {
            editor.putBoolean(ENTRY_ACTION_BAR_PINNED, oldPreferences.getBoolean(ENTRY_ACTION_BAR_PINNED, false));
            editor.putInt(ENTRY_PREFERRED_MODE, oldPreferences.getInt(ENTRY_PREFERRED_MODE, NO_MODE));
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle b = getIntent().getExtras();
        if (b == null) {
            finish();
            return;
        }

        mCameraId = (UUID) b.getSerializable(CAMERA_ID_UUID);
        mCamera = getApp().getHdwResources().getEnabledCameraById(mCameraId);
        if (mCamera == null)
            return; // TODO: #GDM show diagnostic message

        mEncodedCameraPhysicalId = Uri.encode(mCamera.getPhysicalId());

        Log("video activity created, saved state " + savedInstanceState);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        // Remove notification bar
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

        if (!Utils.hasHoneycomb()) {
            // Remove title bar if there is hardware menu button and there is no
            // useful title
            requestWindowFeature(Window.FEATURE_NO_TITLE);
        }

        setContentView(R.layout.activity_video);

        setTitle(mCamera.getName());

        mDetailsView = (TextView) findViewById(R.id.textViewDetail);
        mDetailsView.setBackgroundResource(android.R.color.transparent);
        mDetailsView.setKeepScreenOn(true);

        mVideoView = (HdwVideoView) findViewById(R.id.videoViewCamera);
        mVideoView.setKeepScreenOn(true);
        mVideoView.setOnErrorListener(new MediaPlayer.OnErrorListener() {

            @Override
            public boolean onError(MediaPlayer mp, int what, int extra) {

                if (!mTransportSwitched && mPreferredMode == NO_MODE) {
                    mTransportSwitched = true;
                    if (BuildConfig.DEBUG)
                        Toast.makeText(VideoActivity.this, "Switching transport...", Toast.LENGTH_SHORT).show();

                    Set<MediaTransport> allowed = new HashSet<MediaTransport>();
                    allowed.addAll(mVideoView.allowedTransports());
                    allowed.remove(mVideoView.getTransport());

                    setTransport(MediaTransport.getDefault(allowed));
                    mVideoView.start();
                    return true;
                }

                if (what == MediaPlayer.MEDIA_ERROR_SERVER_DIED) {
                    if (BuildConfig.DEBUG)
                        Toast.makeText(VideoActivity.this, "Retrying...", Toast.LENGTH_SHORT).show();
                    mVideoView.stop();
                    mVideoView.start();
                    return true;
                }
                
                mVideoView.stop();
                new AlertDialog.Builder(VideoActivity.this).setTitle(R.string.error).setIcon(android.R.drawable.ic_dialog_alert)
                        .setMessage(parseMediaPlayerError(what, extra)).setCancelable(true)
                        .setNeutralButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.dismiss();
                            }
                        }).show();
                return true;
            }
        });

        mVideoView.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
            @Override
            public void onPrepared(MediaPlayer mp) {
                mTransportSwitched = true; // do not auto-switch transport on
                                           // error if we have played video
                                           // successfully on current transport
            }
        });

        mVideoView.setOnResolutionChangedListener(new HdwVideoView.OnResolutionChangedListener() {

            @TargetApi(Build.VERSION_CODES.HONEYCOMB)
            @Override
            public void resolutionChanged() {
                if (Utils.hasHoneycomb()) {
                    invalidateOptionsMenu();
                }
                if (mMediaController != null)
                    mMediaController.setResolution(mVideoView.getResolution());
            }

        });

        mVideoView.setVideoUrlResolver(this);

        View offlineLabel = findViewById(R.id.textViewOffline);
        offlineLabel.setBackgroundColor(0x77000000);
        mVideoView.setOfflineLabel(offlineLabel);
        mVideoView.setChunks(mChunks);

        mMediaController = new HdwMediaControllerView(this);
        mMediaController.setAnchorView(findViewById(R.id.videoWindow));
        if (Utils.hasHoneycomb())
            mMediaController.setOnVisibilityChangedListener(new HdwMediaControllerView.OnVisibilityChangedListener() {
                @Override
                public void onVisibilityChanged(boolean visible) {
                    updateActionBarGui(visible, ACTION_BAR_DEFAULT_TIMEOUT);
                }
            });

        mVideoView.setMediaController(mMediaController);
        mVideoView.setCustomHeaders(getApp().getSessionInfo().getCustomHeaders());
        if (getApp().getSessionInfo().getCustomHeaders().isEmpty())
            throw new AssertionError();

        SharedPreferences prefs = preferences();
        mPreferredMode = prefs.getInt(ENTRY_PREFERRED_MODE, NO_MODE);
        Log("Preferred mode is: " + mPreferredMode);
        if (mPreferredMode > NO_MODE && mPreferredMode < MediaTransport.values().length)
            setTransport(MediaTransport.values()[mPreferredMode]);
        else
            setTransport(MediaTransport.getDefault());

        mLiveButton = (ImageView) findViewById(R.id.buttonLiveMode);
        if (mLiveButton != null) {
            if (Utils.hasHoneycomb())
                mLiveButton.setVisibility(View.GONE);
            mLiveButton.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    mVideoView.startLive();
                }
            });
        }

        mArchiveButton = (ImageView) findViewById(R.id.buttonArchiveMode);
        if (mArchiveButton != null) {
            if (Utils.hasHoneycomb())
                mArchiveButton.setVisibility(View.GONE);
            mArchiveButton.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    browseArchive();
                }
            });
        }

        if (savedInstanceState != null) {
            mPlayPos = savedInstanceState.getInt(STORED_PLAY_POS);
            mChunks.merge((ChunkList) savedInstanceState.getSerializable(STORED_CHUNKS));
            mChunksSuccessTime = savedInstanceState.getLong(STORED_SUCCESS_TIME);
            Log("restored state " + debugTime(mPlayPos));
        }

        setArchiveEnabled(mChunks.size() > 0);

        updateCameraResolutions();

        if (Utils.hasHoneycomb()) {
            mActionBarPinned = prefs.getBoolean(ENTRY_ACTION_BAR_PINNED, false);

            // here all buttons will be hidden and action bar will be displayed
            updateActionBarGui(true, ACTION_BAR_DEFAULT_TIMEOUT);

            // here request to hide will be queued
            if (!mActionBarPinned)
                updateActionBarGui(false, ACTION_BAR_DEFAULT_TIMEOUT);
        }
    }

    private String getCameraParameter(final String key) {
        if (mCamera == null || !mCamera.hasParam(key))
            return null;
        Log("get camera parameter: " + key);
        String result = mCamera.getParam(key);
        Log(result);
        return result;
    }
    
    private MediaStreams getMediaStreams() {
        /* Check if resolutions are already set. */
        if (mVideoView.getMediaStreams() != null)
            return mVideoView.getMediaStreams();
        
        final String streams = getCameraParameter(MEDIA_STREAMS_PARAM);
        
        // TODO: #GDM update this parameter on parentIdChanged
        if (streams == null) {
            Log("Camera did not provide mediastreams");
            return null;
        }

        final Gson gson = new Gson();
        try {
            return new MediaStreams(gson.fromJson(streams, MediaStreams.JsonMediaStreamsWrapper.class));
        } catch (com.google.gson.JsonSyntaxException e) {
            e.printStackTrace();
        }
        return null;        
    }
    
    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void updateCameraResolutions() {      
        /* Check if resolutions are already set. */
        if (mVideoView.getMediaStreams() != null)
            return;
               
        final MediaStreams streams = getMediaStreams();
        mVideoView.setMediaStreams(streams);

        mMediaController.updateAllowedResolutions();
        if (Utils.hasHoneycomb())
            invalidateOptionsMenu();
    }

    private CharSequence parseMediaPlayerError(int what, int extra) {

        if (what == MediaPlayer.MEDIA_ERROR_SERVER_DIED)
            return getString(R.string.error_server_died);

        switch (extra) {
        // VideoPlayer standard errors
        case -1000:
            return getString(R.string.error_already_connected);
        case -1001:
            return getString(R.string.error_not_connected);
        case -1002:
            return getString(R.string.error_unknown_host);
        case -1003:
            return getString(R.string.error_cannot_connect);
        case -1004:
            return getString(R.string.error_io);
        case -1005:
            return getString(R.string.error_connection_lost);
        case -1007:
            return getString(R.string.error_malformed_packet);
        case -1008:
            return getString(R.string.error_out_of_range);
        case -1009:
            return getString(R.string.error_buffer_too_small);
        case -1010:
            return getString(R.string.error_unsupported_format);
        case -1011:
            return getString(R.string.error_end_of_stream);
        }

        MediaTransport transport = mVideoView.getTransport();

        ServerInfo serverInfo = getApp().getServerInfo();
        boolean isEc2Server = (serverInfo != null && serverInfo.ProtocolVersion > ServerInfo.PROTOCOL_2_3);

        if (transport == MediaTransport.HTTP && !Utils.hasIcecream())
            return getString(R.string.error_unsupported_http_transport);

        if (!isEc2Server) {
            if (transport == MediaTransport.RTSP && mUsingProxy && !Utils.hasIcecream())
                return getString(R.string.error_unsupported_rtsp_proxy);

            if (transport == MediaTransport.RTSP && mUsingProxy)
                return getString(R.string.error_proxy_not_running);
        }

        if (mVideoView.getResolution().isNative())
            return getString(R.string.error_native_codec);

        if (transport == MediaTransport.RTSP && !Utils.hasIcecream())
            return getString(R.string.error_unsupported_rtsp_udp);

        int id = R.string.error_unknown;
        String result = getString(id);
        if (BuildConfig.DEBUG) {
            result = result + '\n' + mVideoView.getUri();
            Log("reporting error: " + result);
        }
        return result;
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void setTransport(MediaTransport mediaTransport) {
        mVideoView.setTransport(mediaTransport);
        mMediaController.updateAllowedResolutions();
        if (Utils.hasHoneycomb())
            invalidateOptionsMenu();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        for (ChunksLoadingTask task : mChunksLoadingTasks)
            task.cancel(true);
        mChunksLoadingTasks.clear();

        mPlayPos = mVideoView.getCurrentPosition();
        Log("saving state " + debugTime(mPlayPos));
        outState.putInt(STORED_PLAY_POS, mPlayPos);
        outState.putLong(STORED_SUCCESS_TIME, mChunksSuccessTime);
        if (mChunks != null)
            outState.putSerializable(STORED_CHUNKS, mChunks);

        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onPause() {
        mHandler.removeMessages(MESSAGE_PROGRESS);
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mBroadcastReceiver);
        super.onPause();
    }

    @Override
    protected void onResume() {
        System.gc();
        mState = null;

        Log("resumed, state " + debugTime(mPlayPos));
        mVideoView.setCurrentPosition(mPlayPos);

        mHandler.sendEmptyMessage(MESSAGE_PROGRESS);
        updateProgress();
        updateCameraResource();
        LocalBroadcastManager.getInstance(this).registerReceiver(mBroadcastReceiver, new IntentFilter(UiConsts.BCAST_RESOURCES_UPDATED));
        LocalBroadcastManager.getInstance(this).registerReceiver(mBroadcastReceiver, new IntentFilter(UiConsts.BCAST_LOGOFF));

        if (Utils.checkConnection(this))
            mVideoView.start();

        super.onResume();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Log("calendar result " + resultCode);
        if (requestCode == UiConsts.ARCHIVE_POSITION_REQUEST && resultCode == RESULT_OK) {
            mPlayPos = data.getExtras().getInt(UiConsts.ARCHIVE_POSITION);
            Log("calendar data accepted " + debugTime(mPlayPos));
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void updateActionBarGui(boolean visible, long actionBarTimeout) {
        if (visible) {
            ActionBar actionBar = getActionBar();
            if (actionBar != null)
                actionBar.show();
            mDetailsView.setVisibility(View.GONE);

            mHandler.removeMessages(MESSAGE_ACTION_BAR_FADE_OUT);
        } else {
            mHandler.removeMessages(MESSAGE_ACTION_BAR_FADE_OUT);
            if (!mActionBarPinned) {
                Message msg = mHandler.obtainMessage(MESSAGE_ACTION_BAR_FADE_OUT);
                mHandler.sendMessageDelayed(msg, actionBarTimeout);
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {

        final int action = event.getAction();
        switch (action & MotionEvent.ACTION_MASK) {

        case MotionEvent.ACTION_UP: {
            if (/* isInPlaybackState() && */mMediaController != null) {
                mMediaController.show();
            }
            break;
        }
        }
        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_video, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        MenuItem resolutions_item = menu.findItem(R.id.menu_resolutions);
        SubMenu resolutions = resolutions_item.getSubMenu();

        resolutions.clear();

        for (final Resolution r : mVideoView.allowedResolutions()) {
            MenuItem item = resolutions.add(r.toString());
            item.setCheckable(true);
            item.setChecked(r.equals(mVideoView.getResolution()));
            item.setOnMenuItemClickListener(new OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem item) {
                    mMediaController.setResolution(r);
                    return true;
                }
            });

        }

        for (MediaTransport transport : mTransportMenuItems.keySet()) {
            MenuItem transportItem = menu.findItem(mTransportMenuItems.get(transport));
            transportItem.setVisible(mVideoView.allowedTransports().contains(transport));
            if (transportItem.isVisible())
                transportItem.setChecked(mVideoView.getTransport() == transport);
        }

        if (mState != null) {
            MenuItem liveItem = menu.findItem(R.id.menu_video_live);
            liveItem.setChecked(mState.Playing && mState.Position < 0);
            if (mState.Offline) {
                liveItem.setIcon(R.drawable.btn_live);
            } else {
                if (mState.Playing && mState.Position < 0)
                    liveItem.setIcon(R.drawable.btn_live_active);
                else
                    liveItem.setIcon(R.drawable.btn_live);
            }
            liveItem.setEnabled(mState.LiveAllowed);

            MenuItem archiveItem = menu.findItem(R.id.menu_video_archive);
            archiveItem.setChecked(mState.Playing && mState.Position >= 0);
            if (mState.Playing && mState.Position >= 0)
                archiveItem.setIcon(R.drawable.btn_archive_active);
            else
                archiveItem.setIcon(R.drawable.btn_archive);
            archiveItem.setEnabled(mArchiveEnabled);
        }

        MenuItem pinItem = menu.findItem(R.id.menu_pin_actionbar);
        pinItem.setVisible(Utils.hasHoneycomb());
        pinItem.setChecked(mActionBarPinned);
        if (mActionBarPinned)
            pinItem.setIcon(R.drawable.btn_pin_checked);
        else
            pinItem.setIcon(R.drawable.btn_pin);

        return super.onPrepareOptionsMenu(menu);
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        for (Entry<MediaTransport, Integer> entry : mTransportMenuItems.entrySet()) {
            if (entry.getValue() != item.getItemId())
                continue;

            MediaTransport transport = entry.getKey();
            if (!mVideoView.allowedTransports().contains(transport))
                return false;

            Log("changing transport to " + transport.toString());
            setTransport(transport);
            mTransportSwitched = true;
            mPreferredMode = transport.ordinal();
            Log("Preferred mode changed to: " + mPreferredMode);

            savePreferences();

            item.setChecked(true);
            mVideoView.start();
            return true;
        }

        switch (item.getItemId()) {
        case R.id.menu_video_live: {
            mVideoView.startLive();
            return true;
        }
        case R.id.menu_video_archive: {
            browseArchive();
            return true;
        }
        case R.id.menu_pin_actionbar: {
            mActionBarPinned = !mActionBarPinned;
            if (Utils.hasHoneycomb())
                invalidateOptionsMenu();
            savePreferences();
            // here actionbar will be displayed or hidden
            updateActionBarGui(false, ACTION_BAR_UNPIN_TIMEOUT);
            return true;
        }
        default:
            break;
        }
        return super.onOptionsItemSelected(item);
    }

    private void savePreferences() {
        preferencesEditor().putBoolean(ENTRY_ACTION_BAR_PINNED, mActionBarPinned).putInt(ENTRY_PREFERRED_MODE, mPreferredMode).commit();
        Log("Preferred mode saved: " + mPreferredMode);
    }

    private void updateCameraResource() {
        if (mCameraId == null) {
            finish();
            return;
        }
        CameraResource camera = getApp().getHdwResources().getEnabledCameraById(mCameraId);

        if (mCamera != camera)
            mVideoView.stop();

        mCamera = camera;
        if (mCamera == null) {
            finish();
            return;
        }

        boolean offline = (mCamera.isDisabled() || mCamera.getStatus() == Status.Offline || mCamera.getStatus() == Status.Unauthorized);
        mVideoView.setIsOffline(offline);
        updateProgress();
        updateCameraResolutions();
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void setArchiveEnabled(boolean enabled) {
        mArchiveEnabled = enabled;
        mMediaController.setNavigationEnabled(enabled);
        if (Utils.hasHoneycomb())
            invalidateOptionsMenu(); // TODO: move out state changing method
        else if (mArchiveButton != null)
            mArchiveButton.setEnabled(enabled);
    }

    public void updateProgress() {
        if (mCamera == null)
            return;

        tryRequestChunks();

        State newState = new State(mVideoView);
        if (newState.equals(mState))
            return;

        mState = newState;

        StringBuilder sb = new StringBuilder();
        sb.append(mCamera.getName());
        sb.append(" - ");
        if (mState.Offline && mState.Position < 0)
            sb.append(getString(R.string.video_offline));
        else if (mState.Preparing)
            sb.append(getString(R.string.video_loading));
        else if (mState.Playing)
            sb.append(mMediaController.stringForTime(mState.Position));
        else
            sb.append(getString(R.string.video_stopped));
        String title = sb.toString();

        mDetailsView.setText(title);
        if (Utils.hasHoneycomb()) {
            updateActionBar(title);
        } else if (mLiveButton != null && mArchiveButton != null) {
            if (mState.Offline) {
                mLiveButton.setImageResource(R.drawable.btn_live);
            } else {
                if (mState.Playing && mState.Position < 0)
                    mLiveButton.setImageResource(R.drawable.btn_live_active);
                else
                    mLiveButton.setImageResource(R.drawable.btn_live);
            }
            mLiveButton.setEnabled(mState.LiveAllowed);

            if (mState.Playing && mState.Position >= 0)
                mArchiveButton.setImageResource(R.drawable.btn_archive_active);
            else
                mArchiveButton.setImageResource(R.drawable.btn_archive);
        }

    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void updateActionBar(String title) {
        setTitle(title);
        invalidateOptionsMenu();
    }

    private boolean archiveAllowed() {
        UserResource user = getApp().getCurrentUser();
        /* Check if we have already disconnected. */
        if (user == null)
            return false;
        return 
                user.hasPermissions(UserResource.GlobalViewArchivePermission) ||
                user.hasPermissions(UserResource.DeprecatedViewExportArchivePermission); /* 1.4 compatibility */
    }

    private void tryRequestChunks() {
        if (!mChunksLoadingTasks.isEmpty())
            return;

        final long requestTime = System.currentTimeMillis();

        // if chunks loading error was recently, do nothing
        if (mChunksErrorTime > 0 && requestTime - mChunksErrorTime < CHUNKS_ERROR_RETRY_TIMEOUT)
            return;

        if (requestTime - mChunksSuccessTime <= ChunkList.LAST_CUT_SIZE)
            return;

        if (!archiveAllowed())
            return;

        ServerInfo serverInfo = getApp().getServerInfo();
        SessionInfo sessionInfo = getApp().getSessionInfo();

        if (serverInfo == null || sessionInfo == null)
            return;

        for (ServerResource server : getApp().getHdwResources().getAllServersByCamera(mCamera)) {
            if (server.getStatus() != Status.Online)
                continue;

            StringBuilder apiUrl = new StringBuilder();
            apiUrl.append(getApp().getHdwResources().getServerApiUrl(server));
            apiUrl.append("api/RecordedTimePeriods/?periodsType=0&format=json");
            apiUrl.append("&physicalId=");
            apiUrl.append(mEncodedCameraPhysicalId);
            apiUrl.append("&startTime=");
            apiUrl.append(Math.max(mChunksSuccessTime - ChunkList.CHUNK_DETAIL - ChunkList.LAST_CUT_SIZE, 0));
            apiUrl.append("&endTime=");
            apiUrl.append(Long.MAX_VALUE);
            apiUrl.append("&detail=");
            apiUrl.append(ChunkList.CHUNK_DETAIL);

            if (serverInfo.ProtocolVersion < ServerInfo.PROTOCOL_2_3) {
                apiUrl.append("&serverGuid=");
                apiUrl.append(server.getCompatibleId());
            }

            Log("requesting chunks " + apiUrl.toString());

            ChunksLoadingTask task = new ChunksLoadingTask(sessionInfo);
            mChunksLoadingTasks.add(task);
            task.execute(apiUrl.toString());
        }

        // if all servers are offline try to request later
        if (mChunksLoadingTasks.isEmpty())
            mChunksErrorTime = requestTime;

    }

    private void browseArchive() {
        Intent intent = new Intent(VideoActivity.this, CalendarActivity.class);
        intent.putExtra(UiConsts.ARCHIVE_CHUNKS, mChunks);
        intent.putExtra(UiConsts.ARCHIVE_POSITION, mPlayPos);
        startActivityForResult(intent, UiConsts.ARCHIVE_POSITION_REQUEST);
    }

    private class ChunksLoadingTask extends StreamLoadingTask {

        private static final int ERROR_PARSING = Constants.ERROR_BASE - 1;

        private ChunkList mLoadedChunks;
        private long mRequestTime;

        public ChunksLoadingTask(SessionInfo connectInfo) {
            super(connectInfo);
        }

        @Override
        protected void onPreExecute() {
            mRequestTime = System.currentTimeMillis();
        }

        @Override
        protected int parseStream(InputStream stream) {
            String values = Utils.convertStreamToString(stream);

            Log("chunks string size: " + values.length());
            if (isCancelled())
                return Constants.CANCELLED;

            /* Old servers return chunk list as a json object, eg. "([]);" */
            if (values.startsWith("("))
                values = values.substring(1, values.length() - 2);

            Gson gson = new Gson();
            try {
                long[][] parsed = gson.fromJson(values, long[][].class);
                if (isCancelled())
                    return Constants.CANCELLED;
                mLoadedChunks = new ChunkList(parsed);
            } catch (com.google.gson.JsonSyntaxException e) {
                e.printStackTrace();
                return ERROR_PARSING;
            }
            if (isCancelled())
                return Constants.CANCELLED;
            return Constants.SUCCESS;
        }

        @Override
        protected void onPostExecute(Integer result) {
            if (result != Constants.SUCCESS) {
                mChunksErrorTime = mRequestTime;
                return;
            }

            mChunks.merge(mLoadedChunks);

            Log("chunks merged successfully: " + mChunks.size());
            setArchiveEnabled(mChunks.size() > 0);
            mChunksSuccessTime = mRequestTime;
            mChunksLoadingTasks.remove(this);
            if (mChunksLoadingTasks.isEmpty())
                mVideoView.updateCurrentChunk();
        }

    }

    @Override
    public String getVideoUrl(MediaTransport transport, Resolution resolution, long positionMs) {

        class UrlParam {
            String name;
            String value;

            UrlParam(String _name, String _value) {
                name = _name;
                value = _value;
            }
        }

        ServerInfo serverInfo = getApp().getServerInfo();
        SessionInfo sessionInfo = getApp().getSessionInfo();

        if (serverInfo == null || sessionInfo == null || mCamera == null)
            return null;

        /* Server where the archive is stored. */
        ServerResource targetServer = getApp().getHdwResources().getServerByCameraTime(mCamera, positionMs);
        StringBuilder urlBuilder = new StringBuilder();

        mUsingProxy = targetServer != null && !targetServer.isResolved();
        switch (transport) {
        case HTTP:
            urlBuilder.append(getApp().getHdwResources().getServerApiUrl(targetServer));
            urlBuilder.append("media/");
            urlBuilder.append(mEncodedCameraPhysicalId);
            urlBuilder.append(".webm");
            break;
        case RTSP:
            urlBuilder.append(getApp().getHdwResources().getServerUrl(targetServer, "rtsp"));
            urlBuilder.append(mEncodedCameraPhysicalId);
            break;
        }

        List<UrlParam> params = new ArrayList<UrlParam>();

        if (resolution.isNative()) {
            params.add(new UrlParam("stream", String.valueOf(resolution.getStreamIndex())));
        } else {
            params.add(new UrlParam("resolution", resolution.toRawString()));
        }

        if (serverInfo.ProtocolVersion < ServerInfo.PROTOCOL_2_3 && targetServer != null) {
            /* Compatibility mode. */
            params.add(new UrlParam("serverGuid", targetServer.getCompatibleId()));
        } 
        
        if (serverInfo.ProtocolVersion < ServerInfo.PROTOCOL_2_4) {
            params.add(new UrlParam("auth", sessionInfo.getDigest()));
            
        } else {
            params.add(new UrlParam("rt", "1"));
            params.add(new UrlParam("X-runtime-guid", sessionInfo.RuntimeGuid.toString()));                      
            params.add(new UrlParam("auth", getAuthV2()));
        }

        if (transport == MediaTransport.RTSP && !resolution.isNative())
            params.add(new UrlParam("codec", "h263p"));

        if (positionMs >= 0) {
            String pos = Long.toString(positionMs);
            /* RTSP protocol requires position in microseconds. */
            if (transport == MediaTransport.RTSP)
                pos = pos + "000";
            params.add(new UrlParam("pos", pos));
        }

        boolean isFirst = true;
        for (UrlParam param : params) {
            urlBuilder.append(isFirst ? '?' : '&');
            isFirst = false;
            urlBuilder.append(param.name).append('=').append(param.value);
        }

        return urlBuilder.toString();
    }

    private String getAuthV2() {
        HdwApplication app = getApp();
        if (app == null)
            return new String();
        
        SessionInfo sessionInfo = app.getSessionInfo();
        UserResource user = app.getCurrentUser();
              
        if (sessionInfo == null || user == null)
            return new String();
        
        String login = sessionInfo.Login;
        String pass = sessionInfo.Password;
        String realm = user.getRealm();
        
        long serverUSec = (System.currentTimeMillis() + sessionInfo.TimeDiff) * 1000l;
        String nonce = Long.toHexString(serverUSec);
        
        String user_digest = Utils.Md5(login + ":" + realm + ":" + pass);        
        String request = user_digest + ":" + nonce + ":" + Utils.Md5("PLAY:");
        
        String line = sessionInfo.Login + ":" + nonce + ":" + Utils.Md5(request);
                
        String auth = Base64.encodeToString(line.getBytes(), Base64.NO_WRAP);
        return auth;
    }
   
    @Override
    public double getAspectRatio() {
        
        double baseAspectRatio = DEFAULT_VIDEO_ASPECT_RATIO;
        
        double forcedAspectRatio = Utils.safeDoubleFromString(getCameraParameter(FORCED_AR_PARAM), 0.0);
        if (forcedAspectRatio > 0.0) {
            baseAspectRatio = forcedAspectRatio;
        } else {
            final MediaStreams streams = getMediaStreams();
            if (streams != null)
                baseAspectRatio = streams.aspectRatio();
        }

        String videoLayout = getCameraParameter(VIDEO_LAYOUT_PARAM);
        if (videoLayout != null) {
           
            final Matcher matcher = mVideoLayoutPattern.matcher(videoLayout);
            if (!matcher.matches())
                return baseAspectRatio;

            final int width = Integer.parseInt(matcher.group(1));
            final int height = Integer.parseInt(matcher.group(2));
            return (baseAspectRatio * width) / height;
        }
        
        
        return baseAspectRatio;
    }

}
