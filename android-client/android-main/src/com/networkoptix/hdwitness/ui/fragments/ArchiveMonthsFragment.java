package com.networkoptix.hdwitness.ui.fragments;

import java.util.Calendar;
import java.util.GregorianCalendar;

import android.text.format.DateFormat;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.ui.activities.ArchiveActivity.ArchiveDaysActivity;

/**
 * This is the "top-level" fragment, showing a list of items that the
 * user can pick.  Upon picking an item, it takes care of displaying the
 * data to the user as appropriate based on the current UI layout.
 */
public class ArchiveMonthsFragment extends ArchiveListFragment {

	@Override
	protected void generateChunksAndTitles(ChunkList chunks) {
		Calendar calendar = new GregorianCalendar();
		
		int curYear = -1;
		int curMonth = -1;
		ChunkList curChunks = null;
		
		for (int i = 0; i < chunks.size(); i++){
			calendar.setTimeInMillis(1000L * chunks.startByIdx(i));
			int year = calendar.get(Calendar.YEAR);
			int month = calendar.get(Calendar.MONTH);
			if ( year != curYear || month != curMonth){
				curYear = year;
				curMonth = month;
				curChunks = new ChunkList();
				mChunkLists.add(curChunks);
				mTitles.add(DateFormat.format(getActivity().getString(R.string.datetime_months_format), calendar).toString());
			}
			curChunks.add(chunks.startByIdx(i), chunks.lengthByIdx(i));
		}

	}

	@Override
	protected int getChildId() {
		return R.id.archive_days;
	}
	
	@Override
	protected Class<? extends ArchiveListFragment> getChildClass() {
		return ArchiveDaysFragment.class;
	}
	
	@Override
	protected Class<?> getChildActivityClass() {
		return ArchiveDaysActivity.class;
	}
    
}