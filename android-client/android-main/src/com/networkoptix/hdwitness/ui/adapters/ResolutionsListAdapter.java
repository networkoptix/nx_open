package com.networkoptix.hdwitness.ui.adapters;

import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.widget.ArrayAdapter;

import com.networkoptix.hdwitness.common.Resolution;

public class ResolutionsListAdapter extends ArrayAdapter<Resolution> {

	public ResolutionsListAdapter(Context context, ArrayList<Resolution> resolutions) {
		super(context, android.R.layout.simple_spinner_item, resolutions);
		setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        		
	}

    public void update(List<Resolution> resolutions) {
        setNotifyOnChange(false);
        clear();
        for (Resolution r: resolutions)
            add(r);
        notifyDataSetChanged();
    }	

}