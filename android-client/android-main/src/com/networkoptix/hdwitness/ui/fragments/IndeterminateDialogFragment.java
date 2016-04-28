package com.networkoptix.hdwitness.ui.fragments;

import android.app.AlertDialog;
import android.app.Dialog;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import com.networkoptix.hdwitness.R;

public class IndeterminateDialogFragment extends DialogFragment {
	
	private int mTitleResId;
	private int mMsgResId;
	
	public void setTitle(int resId){
		mTitleResId = resId;
	}
	
	public void setMessage(int resId){
		mMsgResId = resId;
	}
	
	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		LayoutInflater inflater = getActivity().getLayoutInflater();
		View v = inflater.inflate(R.layout.dialog_indeterminate, null);
		if (mTitleResId > 0)
			((TextView)v.findViewById(R.id.alertTitle)).setText(mTitleResId);
		else
			v.findViewById(R.id.alertTitle).setVisibility(View.GONE);
		
		if (mMsgResId > 0)
			((TextView)v.findViewById(R.id.textViewMessage)).setText(mMsgResId);
		else
			v.findViewById(R.id.textViewMessage).setVisibility(View.GONE);
	
		final AlertDialog dialog = new AlertDialog.Builder(getActivity())
				.setView(v).setCancelable(true).create();
		dialog.setCanceledOnTouchOutside(false);
		return dialog;		
	}
}