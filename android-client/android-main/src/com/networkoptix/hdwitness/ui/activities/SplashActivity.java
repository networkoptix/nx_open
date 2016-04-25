package com.networkoptix.hdwitness.ui.activities;

import java.lang.ref.WeakReference;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.common.image.BitmapUtils;
import com.networkoptix.hdwitness.ui.HdwApplication;

public class SplashActivity extends HdwActivity {

	class BitmapWorkerTask extends AsyncTask<Integer, Integer, Bitmap> {
		private final WeakReference<ImageView> imageViewReference;
		private int data = 0;

		public BitmapWorkerTask(ImageView imageView) {
			// Use a WeakReference to ensure the ImageView can be garbage
			// collected
			imageViewReference = new WeakReference<ImageView>(imageView);
		}

		// Once complete, see if ImageView is still around and set bitmap.
		@Override
		protected void onPostExecute(Bitmap bitmap) {
			if (imageViewReference != null && bitmap != null) {
				final ImageView imageView = (ImageView) imageViewReference
						.get();
				if (imageView != null) {
					imageView.setImageBitmap(bitmap);
				}
			}
		}

		@Override
		protected Bitmap doInBackground(Integer... params) {
			data = params[0];
			int width = params[1];
			int height = params[2];
			return BitmapUtils.decodeSampledBitmapFromResource(getResources(), data, width, height);
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);

		setContentView(R.layout.activity_splash);
		loadSplash();

		mHandler.sendMessageDelayed(
				mHandler.obtainMessage(HdwApplication.MESSAGE_SPLASH), 3000);
	}

	private void handleSplashV8() {
		Intent intent = new Intent(SplashActivity.this, LogonActivity.class);
		intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(intent);
	}

	
	@TargetApi(Build.VERSION_CODES.HONEYCOMB)
	private void handleSplashV11() {
		Intent intent = new Intent(SplashActivity.this, LogonActivity.class);
		intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(intent);
	}
	
	@Override
	protected void handleMessage(Message msg) {
		switch (msg.what) {
		case HdwApplication.MESSAGE_SPLASH:
		    
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				handleSplashV11();
			} else {
				handleSplashV8();
			}
			break;
		}
		super.handleMessage(msg);
	}


	@SuppressWarnings("deprecation")
	@SuppressLint("NewApi")
	private void loadSplash() {
		System.gc();
		int resId = findViewById(R.id.imageViewSplash_land) != null ? R.drawable.splash_land
				: R.drawable.splash_port;
		ImageView img = (ImageView) findViewById(R.id.imageViewSplash_land);
		if (img == null)
			img = (ImageView) findViewById(R.id.imageViewSplash_port);
		
		int width;
		int height;
		Display display = getWindowManager().getDefaultDisplay();
				
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR2){
			Point size = new Point();
			display.getSize(size);
			width = size.x;
			height = size.y;	
		} else {
			width = display.getWidth();
			height = display.getHeight();
		}
		
	
		BitmapWorkerTask worker = new BitmapWorkerTask(img);
		worker.execute(resId, width, height);
	}
}
