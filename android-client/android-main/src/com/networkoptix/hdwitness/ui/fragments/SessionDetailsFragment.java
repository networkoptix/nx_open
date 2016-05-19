package com.networkoptix.hdwitness.ui.fragments;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.LogonDataManager.LogonEntry;

public class SessionDetailsFragment extends Fragment {

    TextView mHostText;
    TextView mLoginText;
    LogonEntry mEntry;
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        
        View v = inflater.inflate(R.layout.fragment_session_details, container);
        mHostText = (TextView) v.findViewById(R.id.textViewHost);
        mLoginText = (TextView) v.findViewById(R.id.textViewLogin);
        updateForm();
        return v;
    }
    
    public void setEntry(LogonEntry entry) {
        mEntry = entry;
        updateForm();
    }
    
    private void updateForm() {
        if (mEntry == null || mHostText == null || mLoginText == null)
            return;
        mHostText.setText(" " + mEntry.Hostname + ":" + String.valueOf(mEntry.Port));
        mLoginText.setText(" " + mEntry.Login);
    }
    
    
    
}
