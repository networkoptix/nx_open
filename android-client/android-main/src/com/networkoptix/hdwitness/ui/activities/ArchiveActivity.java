package com.networkoptix.hdwitness.ui.activities;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.fragments.ArchiveDaysFragment;
import com.networkoptix.hdwitness.ui.fragments.ArchiveHoursFragment;
import com.networkoptix.hdwitness.ui.fragments.ArchiveListFragment;
import com.networkoptix.hdwitness.ui.fragments.ArchiveMonthsFragment;

public class ArchiveActivity extends FragmentActivity {

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		setResult(RESULT_CANCELED);
		
		setContentView(R.layout.activity_archive);
		
		super.onCreate(savedInstanceState);
		
        if (savedInstanceState == null) {
            // During initial setup, plug in the months fragment.
            ArchiveListFragment months = new ArchiveMonthsFragment();
            months.setArguments(getIntent().getExtras());
            getSupportFragmentManager().beginTransaction().add(R.id.archive_months, months).commit();
        }
	}

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	super.onActivityResult(requestCode, resultCode, data);
    	if (resultCode == RESULT_OK){
    		setResult(resultCode, data);
    		finish();
    	}
    }
	

    /**
     * This is a second activity, to show days of the month
     * when the screen is not large enough to show it all in one activity.
     */

    public static class ArchiveDaysActivity extends FragmentActivity {

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            if (getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_LANDSCAPE) {
                // If the screen is now in landscape mode, we can show the archive in one activity
                finish();
                return;
            }

            if (savedInstanceState == null) {
                // During initial setup, plug in the details fragment.
                ArchiveDaysFragment details = new ArchiveDaysFragment();
                details.setArguments(getIntent().getExtras());
                getSupportFragmentManager().beginTransaction().add(android.R.id.content, details).commit();
            }
        }
        
        @Override
        protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        	super.onActivityResult(requestCode, resultCode, data);
        	if (resultCode == RESULT_OK){
        		setResult(resultCode, data);
        		finish();
        	}
        }
    }
	
    /**
     * This is a third activity, to show hours of the day
     * when the screen is not large enough to show it all in one activity.
     */

    public static class ArchiveHoursActivity extends FragmentActivity {

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            if (getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_LANDSCAPE) {
                // If the screen is now in landscape mode, we can show the archive in one activity
                finish();
                return;
            }

            if (savedInstanceState == null) {
                // During initial setup, plug in the details fragment.
                ArchiveHoursFragment details = new ArchiveHoursFragment();
                details.setArguments(getIntent().getExtras());
                getSupportFragmentManager().beginTransaction().add(android.R.id.content, details).commit();
            }
        }

    }
    
}
