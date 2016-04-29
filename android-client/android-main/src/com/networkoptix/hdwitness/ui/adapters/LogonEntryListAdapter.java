package com.networkoptix.hdwitness.ui.adapters;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import com.networkoptix.hdwitness.ui.LogonDataManager.LogonEntry;

public class LogonEntryListAdapter extends ArrayAdapter<LogonEntry> {

	public LogonEntryListAdapter(Context context, int textViewResourceId) {
		super(context, textViewResourceId);
	}
	
	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		return createView(position, convertView, parent);
	}

	private View createView(int position, View convertView, ViewGroup parent) {
		convertView = super.getView(position, convertView, parent);
		LogonEntry entry = getItem(position);
        TextView t = (TextView) convertView;
        t.setText(entry.title());
		return convertView;
	}

}
