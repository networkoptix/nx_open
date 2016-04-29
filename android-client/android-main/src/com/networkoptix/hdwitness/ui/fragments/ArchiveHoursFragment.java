package com.networkoptix.hdwitness.ui.fragments;

import java.util.Calendar;
import java.util.GregorianCalendar;

import android.app.Activity;
import android.content.Intent;
import android.text.format.DateFormat;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.ui.UiConsts;

public class ArchiveHoursFragment extends ArchiveListFragment {

	@Override
	protected void showItems(int position) {
		Intent intent = new Intent();
		intent.putExtra(UiConsts.ARCHIVE_POSITION, mChunkLists.get(0).startByIdx(position));
		getActivity().setResult(Activity.RESULT_OK, intent);
		getActivity().finish();
	}
	
	@Override
	protected void generateChunksAndTitles(ChunkList chunks) {
		mChunkLists.add(chunks);
		
		
		Calendar calendar = new GregorianCalendar();
    	for (int i = 0; i < chunks.size(); i++) {
    		calendar.setTimeInMillis(1000L * chunks.startByIdx(i));
    		String startStr = DateFormat.format(getActivity().getString(R.string.datetime_hours_format), calendar).toString();
    		calendar.setTimeInMillis(1000L * chunks.endByIdx(i));
    		String endStr = DateFormat.format(getActivity().getString(R.string.datetime_hours_format), calendar).toString();
   			mTitles.add(startStr + getActivity().getString(R.string.archive_divider) + endStr);
    	}
	}
	
}
