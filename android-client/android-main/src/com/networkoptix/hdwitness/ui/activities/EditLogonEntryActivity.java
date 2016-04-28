package com.networkoptix.hdwitness.ui.activities;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;
import android.widget.Toast;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.LogonDataManager.LogonEntry;

public class EditLogonEntryActivity extends Activity {

    public static final int ADD_ENTRY = 0;
    public static final int EDIT_ENTRY = 1;
    public static final String KEY_ENTRY = "entry_key";
    
    
    private TextView mTitleEditor;
    private TextView mHostEditor;
    private TextView mPortEditor;
    private TextView mLoginEditor;
    private TextView mPasswordEditor;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_edit_logon_entry);

        mTitleEditor = (TextView) findViewById(R.id.editTextTitle);
        mHostEditor = (TextView) findViewById(R.id.editTextHost);
        mPortEditor = (TextView) findViewById(R.id.editTextPort);
        mLoginEditor = (TextView) findViewById(R.id.editTextLogin);
        mPasswordEditor = (TextView) findViewById(R.id.editTextPassword);
        
        findViewById(R.id.buttonSave).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (checkAndSave())
                    finish();
            }
        });
        
        findViewById(R.id.buttonCancel).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                setResult(RESULT_CANCELED);
                finish();
            }
        });
        
        if (getIntent().hasExtra(KEY_ENTRY)) 
            fillForm((LogonEntry) getIntent().getSerializableExtra(KEY_ENTRY));
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_edit_entry, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        switch (item.getItemId()) {
        case R.id.menu_logon_entry_save:
            if (checkAndSave())
                finish();
            return true;
        case R.id.menu_logon_entry_cancel:
            setResult(RESULT_CANCELED);
            finish();
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
    }
    
    private void fillForm(LogonEntry entry) {
        if (entry == null)
            return;

        mTitleEditor.setText(entry.Title);
        mHostEditor.setText(entry.Hostname);
        mPortEditor.setText(String.valueOf(entry.Port));
        mLoginEditor.setText(entry.Login);
        mPasswordEditor.setText(entry.Password);
    }
    
    private boolean warnIfEmpty(TextView view, int msgResId) {
        boolean result = view.getText().length() == 0;
        if (result) {
            Toast.makeText(this, msgResId, Toast.LENGTH_SHORT).show();
            view.requestFocus();
        }
        return result;
    }
    
    private boolean checkAndSave() {
        if (warnIfEmpty(mHostEditor, R.string.login_empty_host))
            return false;
        if (warnIfEmpty(mPortEditor, R.string.login_empty_port))
            return false;
        if (warnIfEmpty(mLoginEditor, R.string.login_empty_login))
            return false;
        if (warnIfEmpty(mPasswordEditor, R.string.login_empty_pass))
            return false;
        
        try {
            Integer.parseInt(mPortEditor.getText().toString());
        } catch (NumberFormatException e) {
            Toast.makeText(this, R.string.login_error_port, Toast.LENGTH_LONG).show();
            e.printStackTrace();
            return false;
        }

        LogonEntry entry = new LogonEntry(mTitleEditor.getText().toString(),
                mLoginEditor.getText().toString(),
                mPasswordEditor.getText().toString(),
                mHostEditor.getText().toString(),
                Integer.parseInt(mPortEditor.getText().toString()));
        
        Intent result = new Intent();
        result.putExtra(KEY_ENTRY, entry);
        setResult(RESULT_OK, result);
       
        return true;
    }
    
    
}
