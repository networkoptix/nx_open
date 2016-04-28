package com.networkoptix.hdwitness.ui.fragments;

import java.util.Calendar;
import java.util.GregorianCalendar;

import android.text.format.DateFormat;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.ui.activities.ArchiveActivity.ArchiveHoursActivity;

/**
 * This is the secondary fragment, displaying the details of a particular
 * item.
 */
public class ArchiveDaysFragment extends ArchiveListFragment {
	

    @Override
    protected void generateChunksAndTitles(ChunkList chunks){
		Calendar calendar = new GregorianCalendar();
    	
		int curDay = -1;
		ChunkList curChunks = null;
    	
    	for (int i = 0; i < chunks.size(); i++) {
    		calendar.setTimeInMillis(1000L * chunks.startByIdx(i));
    		int day = calendar.get(Calendar.DAY_OF_MONTH);
    		if ( day != curDay ){
    			curDay = day;
    			curChunks = new ChunkList();
    			mChunkLists.add(curChunks);
    			mTitles.add(DateFormat.format(getActivity().getString(R.string.datetime_days_format), calendar).toString());
    		}
    		curChunks.add(chunks.startByIdx(i), chunks.lengthByIdx(i));
    	}

    }

	@Override
	protected int getChildId() {
		return R.id.archive_hours;
	}
	
	@Override
	protected Class<? extends ArchiveListFragment> getChildClass() {
		return ArchiveHoursFragment.class;
	}
	
	@Override
	protected Class<?> getChildActivityClass() {
		return ArchiveHoursActivity.class;
	}
}