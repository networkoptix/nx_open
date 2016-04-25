package com.networkoptix.hdwitness.ui.activities;

import java.util.Locale;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.support.v4.app.DialogFragment;
import android.view.ActionMode;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.common.network.Constants;
import com.networkoptix.hdwitness.ui.HdwApplication;
import com.networkoptix.hdwitness.ui.LogonDataManager;
import com.networkoptix.hdwitness.ui.LogonDataManager.LogonEntry;
import com.networkoptix.hdwitness.ui.adapters.LogonEntryListAdapter;
import com.networkoptix.hdwitness.ui.fragments.IndeterminateDialogFragment;
import com.networkoptix.hdwitness.ui.fragments.SessionDetailsFragment;

public class LogonActivity extends HdwActivity {

    public static class VersionDialogFragment extends DialogFragment {

        @SuppressLint("InflateParams")
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            LayoutInflater inflater = getActivity().getLayoutInflater();
            View v = inflater.inflate(R.layout.dialog_alert, null);
            ((TextView) v.findViewById(R.id.alertTitle)).setText(R.string.login_version_incompatible_title);
            ((TextView) v.findViewById(R.id.textViewMessage)).setText(R.string.login_version_incompatible_text);

            final AlertDialog dialog = new AlertDialog.Builder(getActivity()).setView(v).setCancelable(true)
                    .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int id) {
                            dialog.dismiss();
                            Intent intent = new Intent(getActivity(), ResourcesActivity.class);
                            startActivity(intent);
                        }
                    }).create();
            dialog.setCanceledOnTouchOutside(false);
            return dialog;
        }

    }

    private static final String TAG_CONNECT_DIALOG = "tag_dialog_ing";
    private static final String TAG_VERSION_DIALOG = "tag_dialog_version";

    private LogonDataManager mLogonDataManager = new LogonDataManager();

    private ListView mEntries;
    private LogonEntryListAdapter mAdapter;
    private SessionDetailsFragment mSessionDetails;
    private ActionMode mActionMode;
    private int mCurrentIndex = -1;

    @Override
    protected void migratePreferences(int oldVersion,
    		SharedPreferences oldPreferences, Editor editor) {
    	super.migratePreferences(oldVersion, oldPreferences, editor);
    	
    	if (oldVersion == 0) {
	    	LogonDataManager dataManager = new LogonDataManager();
	    	dataManager.init(oldPreferences);
	    	dataManager.store(editor);
    	}
    }
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_logon_entries);

        PackageManager manager = getPackageManager();
        PackageInfo info;
        try {
            info = manager.getPackageInfo(getPackageName(), 0);
            setTitle(getTitle() + " " + info.versionName + "." + info.versionCode);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }

        if (BuildConfig.DEBUG)
            setTitle(getTitle() + "[debug]");

        mSessionDetails = (SessionDetailsFragment) getSupportFragmentManager().findFragmentById(
                R.id.session_details_fragment);
        getSupportFragmentManager().beginTransaction().hide(mSessionDetails).commit();

        mAdapter = new LogonEntryListAdapter(this, android.R.layout.simple_list_item_single_choice);
        mEntries = (ListView) findViewById(R.id.logon_entries_list);
        mEntries.setAdapter(mAdapter);
        mEntries.setChoiceMode(ListView.CHOICE_MODE_SINGLE);

        if (Utils.hasHoneycomb()) {
            initCAB();
            
            findViewById(R.id.buttonAdd).setVisibility(View.GONE);
            findViewById(R.id.buttonConnect).setVisibility(View.GONE);
            
        } else {
            mEntries.setOnItemClickListener(new OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parent, View view, int pos, long id) {
                    setCurrentEntry(pos);
                    view.setSelected(true);
                }
            });
            
            findViewById(R.id.buttonConnect).setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    connectEntry(mCurrentIndex);
                }
            });
            
            findViewById(R.id.buttonAdd).setOnClickListener(new OnClickListener() {
    			
    			@Override
    			public void onClick(View v) {
    				addEntry();
    			}
    		});
        }
        registerForContextMenu(mEntries);

        
        mLogonDataManager.init(preferences());
        updateCurrent();

        if (BuildConfig.DEBUG)
            mEntries.setKeepScreenOn(true);

        if (mLogonDataManager.getEntries().isEmpty())
            addEntry();
    }

    private void saveLogonEntries() {
        SharedPreferences.Editor editor = preferencesEditor();
        mLogonDataManager.store(editor);
        boolean success = editor.commit();
        if (success)
            Log("Preferences stored successfully: " + mLogonDataManager.getEntries().size());
        else
            Log("Preferences could not be stored");
    }
        
    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void initCAB() {
        final ActionMode.Callback actionModeCallback = new ActionMode.Callback() {

            // Called when the action mode is created; startActionMode() was
            // called
            @Override
            public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                // Inflate a menu resource providing context menu items
                MenuInflater inflater = mode.getMenuInflater();
                inflater.inflate(R.menu.menu_logon_entry, menu);
                return true;
            }

            // Called each time the action mode is shown. Always called after
            // onCreateActionMode, but
            // may be called multiple times if the mode is invalidated.
            @Override
            public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                return false; // Return false if nothing is done
            }

            // Called when the user selects a contextual menu item
            @Override
            public boolean onActionItemClicked(ActionMode mode, MenuItem item) {

                switch (item.getItemId()) {
                case R.id.menu_logon_entry_connect:
                    connectEntry(mCurrentIndex);
                    mode.finish();
                    return true;
                case R.id.menu_logon_entry_edit:
                    editEntry(mCurrentIndex);
                    mode.finish();
                    return true;
                case R.id.menu_logon_entry_delete:
                    deleteEntry(mCurrentIndex);
                    mode.finish();
                    return true;
                default:
                    return false;
                }
            }

            // Called when the user exits the action mode
            @Override
            public void onDestroyActionMode(ActionMode mode) {
                mActionMode = null;
            }
        };
        mEntries.setOnItemClickListener(new OnItemClickListener() {

            @Override
            public void onItemClick(AdapterView<?> parent, View view, int pos, long id) {
                setCurrentEntry(pos);

                view.setSelected(true);
                // Start the CAB using the ActionMode.Callback defined above
                if (mActionMode == null)
                    mActionMode = startActionMode(actionModeCallback);
            }
        });
    }

    protected void setCurrentEntry(int index) {
        mCurrentIndex = index;
        if (index < 0) {
            getSupportFragmentManager().beginTransaction().hide(mSessionDetails).commit();
            return;
        }

        mSessionDetails.setEntry(mLogonDataManager.getEntries().get(index));
        getSupportFragmentManager().beginTransaction().show(mSessionDetails).commit();
    }

    @Override
    protected void handleMessage(Message msg) {
        switch (msg.what) {
        case HdwApplication.MESSAGE_CONNECTED:
            handleMessageConnected(msg);
            break;
        case HdwApplication.MESSAGE_CONNECT_ERROR:
            handleMessageError(msg);
            break;
        }
        super.handleMessage(msg);
    }

    @Override
    protected void onResumeFragments() {
        super.onResumeFragments();
        updateCurrent();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_logon_entries, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        switch (item.getItemId()) {
        case R.id.menu_logon_add_entry:
            addEntry();
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_logon_entry, menu);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterContextMenuInfo info = (AdapterContextMenuInfo) item.getMenuInfo();
        switch (item.getItemId()) {
        case R.id.menu_logon_entry_connect:
            connectEntry(info.position);
            return true;
        case R.id.menu_logon_entry_edit:
            editEntry(info.position);
            return true;
        case R.id.menu_logon_entry_delete:
            deleteEntry(info.position);
            return true;
        default:
            return super.onContextItemSelected(item);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_OK) {
            LogonEntry entry = (LogonEntry) data.getSerializableExtra(EditLogonEntryActivity.KEY_ENTRY);
            switch (requestCode) {
            case EditLogonEntryActivity.ADD_ENTRY:
                mLogonDataManager.addEntry(entry);
                updateAdapter();
                mCurrentIndex = 0;
                saveLogonEntries();
                break;
            case EditLogonEntryActivity.EDIT_ENTRY:
                mLogonDataManager.updateEntry(mCurrentIndex, entry);
                updateAdapter();
                saveLogonEntries();
                break;
            default:
                break;
            }
        }
    }

    private void handleMessageConnected(Message msg) {
        DialogFragment dialog = (DialogFragment) getSupportFragmentManager().findFragmentByTag(TAG_CONNECT_DIALOG);
        if (dialog != null) {
            dialog.dismiss();
            getSupportFragmentManager().beginTransaction().detach(dialog).commit();
        }
        
        ServerInfo serverInfo = getApp().getServerInfo();
        Log("Server info received: " + serverInfo.toString());
        
        if (!serverInfo.Compatible) {
            Log("Incompatible server");
            getSupportFragmentManager().beginTransaction().add(new VersionDialogFragment(), TAG_VERSION_DIALOG)
                    .commit();
            return;
        }

        Log("Connected successfully!");
        Intent intent = new Intent(LogonActivity.this, ResourcesActivity.class);
        startActivity(intent);
    }

    private void handleMessageError(Message msg) {
        DialogFragment dialog = (DialogFragment) getSupportFragmentManager().findFragmentByTag(TAG_CONNECT_DIALOG);
        if (dialog != null) {
            dialog.dismiss();
            getSupportFragmentManager().beginTransaction().detach(dialog).commit();
        }
        
        String errText = getString(R.string.login_error_unknown);
        switch (msg.arg1) {
        case Constants.URI_ERROR:
            errText = getString(R.string.login_error_path);
            break;
        case Constants.PROTOCOL_ERROR:
            errText = getString(R.string.login_error_protocol);
            break;
        case Constants.IO_ERROR:
            errText = getString(R.string.login_error_connection);
            break;
        case Constants.CREDENTIALS_ERROR:
            errText = getString(R.string.login_error_credentials);
            break;
        case Constants.BRAND_ERROR:
            errText = getString(R.string.login_error_brand);
            break;
        }
        
        Log("Connection error " + msg.arg1);
        Log(errText);
        
        Toast.makeText(LogonActivity.this, errText, Toast.LENGTH_LONG).show();
    }

    private void addEntry() {
        Intent intent = new Intent(LogonActivity.this, EditLogonEntryActivity.class);
        startActivityForResult(intent, EditLogonEntryActivity.ADD_ENTRY);
    }

    private void editEntry(int index) {
        if (index < 0)
            return;
        setCurrentEntry(index);
        Intent intent = new Intent(LogonActivity.this, EditLogonEntryActivity.class);
        intent.putExtra(EditLogonEntryActivity.KEY_ENTRY, mLogonDataManager.getEntries().get(index));
        startActivityForResult(intent, EditLogonEntryActivity.EDIT_ENTRY);
    }

    private void deleteEntry(int index) {
        if (index < 0)
            return;

        LogonEntry entry = mLogonDataManager.getEntries().get(index);

        final int idx = index;
        final String title = entry.title();

        new AlertDialog.Builder(this).setTitle(getString(R.string.app_name))
                .setMessage(getString(R.string.login_delete_confirm) + "\n" + title)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        mLogonDataManager.removeEntry(idx);
                        if (mCurrentIndex == idx)
                            mCurrentIndex = -1;
                        updateCurrent();
                        saveLogonEntries();
                    }
                }).setNegativeButton(android.R.string.cancel, null).show();
    }

    private void updateAdapter() {
        mAdapter.clear();
        for (LogonEntry e : mLogonDataManager.getEntries())
            mAdapter.add(e);
        mAdapter.notifyDataSetChanged();
    }

    private void updateCurrent() {
        updateAdapter();
        setCurrentEntry(mCurrentIndex);
        mEntries.setSelection(mCurrentIndex);
    }

    private void connectEntry(int index) {
        if (index < 0)
            return;
        doLogin(mLogonDataManager.getEntries().get(index));
    }

    private void doLogin(LogonEntry entry) {
        Log("Trying to connect");
        if (!Utils.checkConnection(this)) {
            Log("No internet connection found!");
            return;
        }

        final SessionInfo data = new SessionInfo();
        data.Title = entry.Title;
        data.Login = entry.Login.toLowerCase(Locale.getDefault());
        data.Password = entry.Password;
        data.Hostname = entry.Hostname;
        data.Port = entry.Port;
        data.fillCustomHeaders(this);
        Log("Connecting to: " + data.toDetailedString()); 

        IndeterminateDialogFragment dialog = new IndeterminateDialogFragment();
        dialog.setTitle(R.string.login_progress_caption);
        dialog.setMessage(R.string.login_connecting);
        getSupportFragmentManager().beginTransaction().add(dialog, TAG_CONNECT_DIALOG).commit();
        getApp().requestConnect(mHandler, data);
    }

}
