package com.networkoptix.hdwitness.ui.activities;

import java.util.Timer;
import java.util.TimerTask;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Toast;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.ui.UiConsts;
import com.networkoptix.hdwitness.ui.fragments.IndeterminateDialogFragment;
import com.networkoptix.hdwitness.ui.fragments.ResourcesGridFragment;
import com.networkoptix.hdwitness.ui.fragments.ResourcesTreeFragment;

public class ResourcesActivity extends HdwActivity {

    private static final String TAG_DIALOG = "ResourcesDialog";

    private static final String ENTRY_SHOW_OFFLINE = "show_offline";

    private volatile boolean mLogoutOffered = false;
    private boolean mShowOffline = true;
    private Timer mLogoutTimer;

    private BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(UiConsts.BCAST_LOGOFF)) {
                DialogFragment dialog = (DialogFragment) getSupportFragmentManager().findFragmentByTag(TAG_DIALOG);
                if (dialog != null) {
                    dialog.dismiss();
                    getSupportFragmentManager().beginTransaction().detach(dialog).commitAllowingStateLoss();
                }

                finish();
            } else if (intent.getAction().equals(UiConsts.BCAST_RESOURCES_UPDATED)) {
                if (getApp().getCurrentUser() == null)
                    return; // not all resources are received

                DialogFragment dialog = (DialogFragment) getSupportFragmentManager().findFragmentByTag(TAG_DIALOG);
                if (dialog != null) {
                    dialog.dismiss();
                    getSupportFragmentManager().beginTransaction().detach(dialog).commitAllowingStateLoss();
                }
            }
        }
    };

    public static class ResourcesDialog extends IndeterminateDialogFragment {

        @Override
        public void onCancel(DialogInterface dialog) {
            super.onCancel(dialog);
            ((HdwActivity) getActivity()).getApp().logoff();
            getActivity().finish();
        }
    }

    private class ResourcesPagerAdapter extends FragmentPagerAdapter {

        public static final int MODE_TREE = 0;
        public static final int MODE_GRID = 1;
        public static final int MODE_COUNT = 2;

        public ResourcesPagerAdapter(FragmentManager fm) {
            super(fm);
        }

        @Override
        public int getCount() {
            return MODE_COUNT;
        }

        @Override
        public Fragment getItem(int position) {
            switch (position) {
            case MODE_TREE:
                return new ResourcesTreeFragment();
            case MODE_GRID:
                return new ResourcesGridFragment();
            }
            return null;
        }
    }

    @Override
    protected void migratePreferences(int oldVersion, SharedPreferences oldPreferences, Editor editor) {
        super.migratePreferences(oldVersion, oldPreferences, editor);

        if (oldVersion == 0) {
            editor.putBoolean(ENTRY_SHOW_OFFLINE, oldPreferences.getBoolean(ENTRY_SHOW_OFFLINE, true));
        }

    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mShowOffline = preferences().getBoolean(ENTRY_SHOW_OFFLINE, mShowOffline);

        setContentView(R.layout.activity_resources);
        ResourcesPagerAdapter adapter = new ResourcesPagerAdapter(getSupportFragmentManager());

        final ViewPager pager = (ViewPager) findViewById(R.id.pager);
        pager.setAdapter(adapter);

        // Watch for button clicks.
        {
            View button = findViewById(R.id.mode_tree);
        
            button.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    pager.setCurrentItem(ResourcesPagerAdapter.MODE_TREE);
                }
            });
        }
        
        {
            View button = findViewById(R.id.mode_grid);
            button.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    pager.setCurrentItem(ResourcesPagerAdapter.MODE_GRID);
                }
            });
        }
        
        {
            View button = findViewById(R.id.promoteLayout);
            button.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.setData(Uri.parse(getString(R.string.promote_app_url)));
                    startActivity(intent);
                }
            });
        }
        
        {
            View promoteView = findViewById(R.id.promoteLayout);
            ServerInfo serverInfo = getApp().getServerInfo();
            if (Utils.hasJellyBean() && serverInfo != null && serverInfo.ProtocolVersion >= ServerInfo.PROTOCOL_2_5)
                promoteView.setVisibility(View.VISIBLE);
            else
                promoteView.setVisibility(View.GONE);
        }

        if (getApp().getSessionInfo() == null)
            finish();
        else
            setTitle(getApp().getSessionInfo().title());

        if (BuildConfig.DEBUG)
            pager.setKeepScreenOn(true);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        preferencesEditor().putBoolean(ENTRY_SHOW_OFFLINE, mShowOffline).commit();
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onResume() {
        LocalBroadcastManager.getInstance(this).registerReceiver(mBroadcastReceiver,
                new IntentFilter(UiConsts.BCAST_RESOURCES_UPDATED));
        LocalBroadcastManager.getInstance(this).registerReceiver(mBroadcastReceiver,
                new IntentFilter(UiConsts.BCAST_LOGOFF));
        super.onResume();
    }

    @Override
    protected void onResumeFragments() {
        super.onResumeFragments();
        if (getApp().getCurrentUser() != null)
            return;

        // progress dialog is required
        if (getSupportFragmentManager().findFragmentByTag(TAG_DIALOG) == null) {
            ResourcesDialog dialog = new ResourcesDialog();
            dialog.setTitle(R.string.resources_loading_title);
            dialog.setMessage(R.string.resources_loading_text);
            getSupportFragmentManager().beginTransaction().add(dialog, TAG_DIALOG).commit();
        }
    }

    @Override
    protected void onPause() {
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mBroadcastReceiver);
        super.onPause();
    }

    @Override
    public void onBackPressed() {
        if (!mLogoutOffered) {
            Toast.makeText(this, R.string.resources_logout_offer, Toast.LENGTH_SHORT).show();
            mLogoutOffered = true;
            if (null != mLogoutTimer)
                mLogoutTimer.cancel();
            mLogoutTimer = new Timer();
            mLogoutTimer.schedule(new TimerTask() {
                @Override
                public void run() {
                    mLogoutOffered = false;
                }
            }, 2000);
        } else {
            if (mLogoutTimer != null) {
                mLogoutTimer.cancel();
                mLogoutTimer = null;
            }
            getApp().logoff();
            finish();
        }

    }

    public boolean getShowOfflineResources() {
        return mShowOffline;
    }

    public void setShowOfflineResources(boolean value) {
        if (mShowOffline == value)
            return;
        mShowOffline = value;
        Intent intent = new Intent(UiConsts.BCAST_SHOW_OFFLINE_UPDATED);
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent);
    }
}
