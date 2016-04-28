/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.networkoptix.hdwitness.common;

import java.util.Arrays;
import java.util.UUID;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.StrictMode;
import android.util.Log;
import android.widget.Toast;

import com.networkoptix.hdwitness.R;

/**
 * Class containing some static utility methods.
 */
public final class Utils {
    private Utils() {};

    @TargetApi(11)
    public static void enableStrictMode() {
        if (Utils.hasGingerbread()) {
            StrictMode.ThreadPolicy.Builder threadPolicyBuilder =
                    new StrictMode.ThreadPolicy.Builder()
                            .detectAll()
                            .penaltyLog();
            StrictMode.VmPolicy.Builder vmPolicyBuilder =
                    new StrictMode.VmPolicy.Builder()
                            .detectAll()
                            .penaltyLog();

            if (Utils.hasHoneycomb()) {
                threadPolicyBuilder.penaltyFlashScreen();
                //TODO: #gdm
             /*   vmPolicyBuilder
                        .setClassInstanceLimit(ImageGridActivity.class, 1)
                        .setClassInstanceLimit(ImageDetailActivity.class, 1);*/
            }
            StrictMode.setThreadPolicy(threadPolicyBuilder.build());
            StrictMode.setVmPolicy(vmPolicyBuilder.build());
        }
    }

    public static boolean hasGingerbread() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD;
    }

    public static boolean hasHoneycomb() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB;
    }

    public static boolean hasHoneycombMR1() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1;
    }

    public static boolean hasIcecream() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH;
    }
    
    public static boolean hasJellyBean() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;
    }
    
    public static boolean hasJellyBeanMR2() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2;
    }
    
    public static boolean hasKitkat() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;
    }
    
    /**
    * Simple network connection check.
    *
    * @param context
    * @return True if a network connection exists
    */
    public static boolean checkConnection(Context context) {
        final ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        final NetworkInfo networkInfo = cm.getActiveNetworkInfo();
        if (networkInfo == null || !networkInfo.isConnectedOrConnecting()) {
            Toast.makeText(context, R.string.no_network_connection_toast, Toast.LENGTH_LONG).show();
            Log.e("Utils", "checkConnection - no connection found");
            return false;
        }
        return true;
    }

	public static String convertStreamToString(java.io.InputStream is) {
		try {
			return new java.util.Scanner(is).useDelimiter("\\A").next();
		} catch (java.util.NoSuchElementException e) {
			return "";
		}
	}
	
	/**
	 * Creates UUID from its string representation with braces.
	 * @param value Standard UUID representation with braces.
	 * @return UUID
	 */
    public static UUID uuid(String value) {
        final UUID nullUuid = new UUID(0, 0);
        
        if (value == null)
            return nullUuid;
        
        if (value.length() != 36 && value.length() != 38)
            return nullUuid;
        
        if (value.startsWith("{"))
            return UUID.fromString(value.substring(1, 37));
        return UUID.fromString(value);        
    }
   
    @TargetApi(Build.VERSION_CODES.GINGERBREAD)
    public static int binarySearch(int[] array, int startIdx, int length, int value) {
        return Utils.hasGingerbread() 
                ? Arrays.binarySearch(array, startIdx, length, value)
                : binarySearchApi8(array, startIdx, length,
                value);
    }

    private static int binarySearchApi8(int[] array, int startIdx, int length,
            int value) {
        int lo = startIdx;
        int hi = length - 1;

        while (lo <= hi) {
            int mid = (lo + hi) >>> 1;
            int midVal = array[mid];

            if (midVal < value) {
                lo = mid + 1;
            } else if (midVal > value) {
                hi = mid - 1;
            } else {
                return mid;  // value found
            }
        }
        return ~lo;  // value not present
    }
    
    @TargetApi(Build.VERSION_CODES.GINGERBREAD)
    public static int[] copyOf(int[] other, int length) {
        return Utils.hasGingerbread() ? Arrays.copyOf(other, length)
                : copyOfApi8(other, length);
    }

    private static int[] copyOfApi8(int[] other, int length) {
        int[] result = new int[length];
        for (int i = 0; i < other.length; i++)
            result[i] = other[i];
        for (int i = other.length; i < length; i++)
            result[i] = 0;

        return result;
    }

    public static String Md5(String md5) {
        try {
            java.security.MessageDigest md = java.security.MessageDigest.getInstance("MD5");
            byte[] array = md.digest(md5.getBytes());
            StringBuffer sb = new StringBuffer();
            for (int i = 0; i < array.length; ++i) {
              sb.append(Integer.toHexString((array[i] & 0xFF) | 0x100).substring(1,3));
           }
            return sb.toString();
        } catch (java.security.NoSuchAlgorithmException e) {
        }
        return null;
    }

    public static double safeDoubleFromString(String value, double defaultValue) {
        if (value == null || value.length() == 0)
            return defaultValue;
        
        double result = defaultValue;
        try {
            result = Double.parseDouble(value);
        } catch (NumberFormatException e) {
            return defaultValue;
        }
        
        if (Double.isInfinite(result) || Double.isNaN(result))
            return defaultValue;
        
        return result;
        
    }
}
