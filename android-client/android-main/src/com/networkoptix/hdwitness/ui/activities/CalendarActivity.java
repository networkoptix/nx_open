package com.networkoptix.hdwitness.ui.activities;

import java.util.Calendar;
import java.util.GregorianCalendar;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.text.format.DateFormat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.DatePicker;
import android.widget.TextView;
import android.widget.TimePicker;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.Chunk;
import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.ui.UiConsts;

public class CalendarActivity extends HdwActivity {

    private static final String ENTRY_USE_24_HOUR = "use_24_hour";
    private static final String STORED_FOCUS = "stored_focus";
    private static final String STORED_HOURS = "stored_hours";
    private static final String STORED_MINUTES = "stored_minutes";

    private static final String CRLF = System.getProperty("line.separator");
    private static final String TAG_DIALOG = "tag_dialog_invalid_chunk";

    private int mFocusIndex = -1;
    private TimePicker mTimePicker;
    private boolean mUse24hour;
    private int mHours;
    private int mMinutes;

    public static class AlertDialogFragment extends DialogFragment {

        private static final String STORED_TITLE = "stored_title";
        private static final String STORED_TIME = "stored_time";
        private static final String STORED_MESSAGE = "stored_message";

        private int mTitleResId;
        private int mTime;
        private String mMessage;

        public void setTitle(int resId) {
            mTitleResId = resId;
        }

        public void setTime(int time) {
            mTime = time;
        }

        public void setMessage(String message) {
            mMessage = message;
        }

        @SuppressLint("NewApi")
        private void restoreInstanceState(Bundle savedInstanceState) {
            mTitleResId = savedInstanceState.getInt(STORED_TITLE, mTitleResId);
            mTime = savedInstanceState.getInt(STORED_TIME, mTime);
            CharSequence message = savedInstanceState.getCharSequence(STORED_MESSAGE);
            if (message != null)
                mMessage = message.toString();
            else
                mMessage = "";
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {

            if (savedInstanceState != null) {
                restoreInstanceState(savedInstanceState);
            }

            LayoutInflater inflater = getActivity().getLayoutInflater();
            View v = inflater.inflate(R.layout.dialog_alert, null);
            ((TextView) v.findViewById(R.id.alertTitle)).setText(mTitleResId);
            ((TextView) v.findViewById(R.id.textViewMessage)).setText(mMessage);

            final AlertDialog dialog = new AlertDialog.Builder(getActivity()).setView(v).setCancelable(true)
                    .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int id) {
                            dialog.dismiss();
                            Intent intent = new Intent();
                            intent.putExtra(UiConsts.ARCHIVE_POSITION, mTime);
                            getActivity().setResult(Activity.RESULT_OK, intent);
                            getActivity().finish();
                        }
                    }).setNegativeButton(android.R.string.no, new DialogInterface.OnClickListener() {

                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                        }
                    }).create();
            dialog.setCanceledOnTouchOutside(false);
            return dialog;
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            super.onSaveInstanceState(outState);

            outState.putInt(STORED_TITLE, mTitleResId);
            outState.putInt(STORED_TIME, mTime);
            outState.putCharSequence(STORED_MESSAGE, mMessage);
        }

    }

    @Override
    protected void migratePreferences(int oldVersion,
    		SharedPreferences oldPreferences, Editor editor) {
    	super.migratePreferences(oldVersion, oldPreferences, editor);
    	
    	if (oldVersion == 0) {
    		editor.putBoolean(ENTRY_USE_24_HOUR, oldPreferences.getBoolean(ENTRY_USE_24_HOUR, true));
    	}
    }
    
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_calendar);
        setResult(RESULT_CANCELED);

        int seconds = getIntent().getExtras().getInt(UiConsts.ARCHIVE_POSITION, -1);
        long millis = (long) (seconds) * 1000l;

        final ChunkList chunks = (ChunkList) getIntent().getExtras().getSerializable(UiConsts.ARCHIVE_CHUNKS);

        Calendar calendar = new GregorianCalendar();
        if (seconds >= 0)
            calendar.setTimeInMillis(millis);
        mHours = calendar.get(Calendar.HOUR_OF_DAY);
        mMinutes = calendar.get(Calendar.MINUTE);
        mUse24hour = preferences().getBoolean(ENTRY_USE_24_HOUR, true);
        mTimePicker = (TimePicker) findViewById(R.id.timePickerArchive);

        final DatePicker datePicker = (DatePicker) findViewById(R.id.datePickerArchive);
        datePicker.updateDate(calendar.get(Calendar.YEAR), calendar.get(Calendar.MONTH),
                calendar.get(Calendar.DAY_OF_MONTH));

        CheckBox cbUse24Hour = (CheckBox) findViewById(R.id.checkBox24HourClock);
        cbUse24Hour.setChecked(mUse24hour);

        cbUse24Hour.setOnCheckedChangeListener(new OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                int hour = mTimePicker.getCurrentHour();
                mTimePicker.setIs24HourView(isChecked);
                mTimePicker.setCurrentHour(hour);
                mUse24hour = isChecked;
            }
        });

        findViewById(R.id.buttonAccept).setOnClickListener(new OnClickListener() {

            public void onClick(View v) {
                Calendar calendar = new GregorianCalendar();
                calendar.set(datePicker.getYear(), datePicker.getMonth(), datePicker.getDayOfMonth(),
                        mTimePicker.getCurrentHour(), mTimePicker.getCurrentMinute(), 0);
                long millis = calendar.getTimeInMillis();
                int position = (int) (millis / 1000);
                Chunk selected = chunks.nearest(position);

                if (millis > System.currentTimeMillis())
                    showLiveDialog(selected);
                else if (selected.contains(position)
                        || ((position > selected.start) && (position - selected.start < 60))) {
                    Intent intent = new Intent();
                    intent.putExtra(UiConsts.ARCHIVE_POSITION, position);
                    setResult(Activity.RESULT_OK, intent);
                    finish();
                } else
                    showAcceptionDialog(selected);
            }
        });

        findViewById(R.id.buttonAdvanced).setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Intent intent = new Intent(CalendarActivity.this, ArchiveActivity.class);
                intent.putExtras(getIntent().getExtras());
                startActivityForResult(intent, UiConsts.ARCHIVE_POSITION_REQUEST);
            }
        });
    }
    
    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putInt(STORED_FOCUS, mFocusIndex);
        outState.putInt(STORED_HOURS, mTimePicker.getCurrentHour());
        outState.putInt(STORED_MINUTES, mTimePicker.getCurrentMinute());
        outState.putBoolean(ENTRY_USE_24_HOUR, mUse24hour);
        
        preferencesEditor().putBoolean(ENTRY_USE_24_HOUR, mUse24hour).commit();
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        if (savedInstanceState == null)
            return;
        mHours = savedInstanceState.getInt(STORED_HOURS, mHours);
        mMinutes = savedInstanceState.getInt(STORED_MINUTES, mMinutes);
        mFocusIndex = savedInstanceState.getInt(STORED_FOCUS, -1);
        mUse24hour = savedInstanceState.getBoolean(ENTRY_USE_24_HOUR, true);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mTimePicker.setIs24HourView(mUse24hour);
        mTimePicker.setCurrentHour(mHours);
        mTimePicker.setCurrentMinute(mMinutes);

        if (mFocusIndex > 0)
            mTimePicker.getChildAt(mFocusIndex).setFocusable(true);

    }

    private String buildChunkDescription(Chunk c) {
        String startStr = DateFormat.format(getString(R.string.datetime_full_format), 1000L * c.start).toString();
        String endStr = DateFormat.format(getString(R.string.datetime_full_format), 1000L * c.end).toString();
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(getString(R.string.calendar_from));
        stringBuilder.append(startStr);
        stringBuilder.append(CRLF);
        stringBuilder.append(getString(R.string.calendar_to));
        stringBuilder.append(endStr);
        String description = stringBuilder.toString();
        return description;
    }

    protected void showInvalidChunkDialog(final String message, final int time) {
        AlertDialogFragment dialog = new AlertDialogFragment();
        dialog.setTitle(R.string.calendar_missing_time);
        dialog.setMessage(message);
        dialog.setTime(time);
        getSupportFragmentManager().beginTransaction().add(dialog, TAG_DIALOG).commit();
    }

    protected void showAcceptionDialog(final Chunk selected) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(getString(R.string.calendar_not_recorded));
        stringBuilder.append(CRLF);
        stringBuilder.append(getString(R.string.calendar_offer_nearest));
        stringBuilder.append(CRLF);
        stringBuilder.append(buildChunkDescription(selected));
        showInvalidChunkDialog(stringBuilder.toString(), selected.start);

    }

    protected void showLiveDialog(final Chunk selected) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(getString(R.string.calendar_future_time_selected));
        stringBuilder.append(CRLF);
        stringBuilder.append(getString(R.string.calendar_offer_last));
        stringBuilder.append(CRLF);
        stringBuilder.append(buildChunkDescription(selected));
        showInvalidChunkDialog(stringBuilder.toString(), selected.start);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_OK) {
            setResult(resultCode, data);
            finish();
        }
    }
}
