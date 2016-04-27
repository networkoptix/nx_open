package com.networkoptix.hdwitness.ui.activities;

import java.lang.ref.WeakReference;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Message;
import android.preference.PreferenceManager;
import android.support.v4.app.FragmentActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.HdwApplication;
import com.networkoptix.hdwitness.ui.PauseHandler;

public abstract class HdwActivity extends FragmentActivity {

    private static String PREFERENCES_KEY = "com.networkoptix.Preferences";
    private static String PREFERENCES_VERSION_KEY = ".VERSION";
    private static int PREFERENCES_VERSION = 1;

    private static final int COLLECT_LOG_UID = Menu.FIRST + 101;

    static class HdwActivityHandler extends PauseHandler {
        private final WeakReference<HdwActivity> mActivity;

        public HdwActivityHandler(HdwActivity activity) {
            mActivity = new WeakReference<HdwActivity>(activity);
        }

        @Override
        public void processMessage(Message msg) {
            HdwActivity activity = mActivity.get();
            if (activity == null)
                return;
            activity.handleMessage(msg);
        }

        @Override
        protected boolean storeMessage(Message message) {
            return true; // all messages should be stored
        }
    }

    protected final HdwActivityHandler mHandler = new HdwActivityHandler(this);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        checkPreferencesVersion();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (BuildConfig.DEBUG) {
            MenuItem collectLog = menu.add(Menu.NONE, COLLECT_LOG_UID, Menu.NONE, R.string.collect_log);
            collectLog.setVisible(true);
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        switch (item.getItemId()) {
        case COLLECT_LOG_UID: {
            collectAndSendLog();
            return true;
        }
        default:
            break;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onPause() {
        mHandler.pause();
        super.onPause();
    }

    @Override
    protected void onResumeFragments() {
        super.onResumeFragments();
        mHandler.resume();
    }

    protected void Log(String message) {
        if (BuildConfig.DEBUG)
            Log.d(this.getLocalClassName(), message);
    }

    private String versionKey() {
        return this.getLocalClassName() + PREFERENCES_VERSION_KEY;
    }

    private void checkPreferencesVersion() {
        int version = preferences().getInt(versionKey(), 0);
        if (version < PREFERENCES_VERSION) {
            SharedPreferences oldPreferences = version == 0 ? PreferenceManager.getDefaultSharedPreferences(this)
                    : preferences();

            SharedPreferences.Editor editor = preferencesEditor();
            migratePreferences(version, oldPreferences, editor);
            editor.putInt(versionKey(), PREFERENCES_VERSION);
            boolean result = editor.commit();
            if (!result)
                Log("Preferences version could not be saved");
        }

    }

    /**
     * Method for user preferences migration between program versions
     * 
     * @param oldVersion
     *            - version of the old preferences.
     * @param oldPreferences
     *            - old preferences.
     * @param editor
     *            - editor to save updated values. Commit should be called here.
     * @return
     */
    protected void migratePreferences(int oldVersion, SharedPreferences oldPreferences, Editor editor) {
    }

    public HdwApplication getApp() {
        return (HdwApplication) getApplication();
    }

    protected void handleMessage(Message msg) {

    }

    protected SharedPreferences preferences() {
        return getSharedPreferences(PREFERENCES_KEY, MODE_PRIVATE);
    }

    protected SharedPreferences.Editor preferencesEditor() {
        return getSharedPreferences(PREFERENCES_KEY, MODE_PRIVATE).edit();
    }

    protected static final String EXTRA_SEND_INTENT_ACTION = "com.xtralogic.logcollector.intent.extra.SEND_INTENT_ACTION";//$NON-NLS-1$
    protected static final String EXTRA_DATA = "com.xtralogic.logcollector.intent.extra.DATA";//$NON-NLS-1$
    protected static final String EXTRA_FORMAT = "com.xtralogic.logcollector.intent.extra.FORMAT";//$NON-NLS-1$

    protected void collectAndSendLog() {
        final Intent intent = new Intent(this, SendLogActivity.class);

        intent.putExtra(EXTRA_SEND_INTENT_ACTION, Intent.ACTION_SEND);
        final String email = "";
        intent.putExtra(EXTRA_DATA, Uri.parse("mailto:" + email));
        intent.putExtra(Intent.EXTRA_SUBJECT, "Application failure report");

        intent.putExtra(EXTRA_FORMAT, "time");

        startActivity(intent);
    }

}
