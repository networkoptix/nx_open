package com.networkoptix.hdwitness.common;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.networkoptix.hdwitness.R;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;

public final class DeviceInfo {

    public static String getVersionNumber(Context context) 
    {
        String version = "?";
        try
        {
            PackageInfo packageInfo = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            version = packageInfo.versionName + "." + packageInfo.versionCode;
        }
        catch (PackageManager.NameNotFoundException e){};
        
        return version;
    }
    
    public static String getFormattedKernelVersion()
    {
        String procVersionStr;

        try {
            BufferedReader reader = new BufferedReader(new FileReader("/proc/version"), 256);
            try {
                procVersionStr = reader.readLine();
            } finally {
                reader.close();
            }

            final String PROC_VERSION_REGEX =
                "\\w+\\s+" + /* ignore: Linux */
                "\\w+\\s+" + /* ignore: version */
                "([^\\s]+)\\s+" + /* group 1: 2.6.22-omap1 */
                "\\(([^\\s@]+(?:@[^\\s.]+)?)[^)]*\\)\\s+" + /* group 2: (xxxxxx@xxxxx.constant) */
                "\\([^)]+\\)\\s+" + /* ignore: (gcc ..) */
                "([^\\s]+)\\s+" + /* group 3: #26 */
                "(?:PREEMPT\\s+)?" + /* ignore: PREEMPT (optional) */
                "(.+)"; /* group 4: date */

            Pattern p = Pattern.compile(PROC_VERSION_REGEX);
            Matcher m = p.matcher(procVersionStr);

            if (!m.matches()) 
                return "Unavailable";
            
            if (m.groupCount() < 4) 
                return "Unavailable";
            
            return (new StringBuilder(m.group(1)).append("\n").append(
                        m.group(2)).append(" ").append(m.group(3)).append("\n")
                        .append(m.group(4))).toString();
        } catch (IOException e) {  
            return "Unavailable";
        }
    }

    public static String getUserAgent(Context context) {
        String brand = context.getString(R.string.app_name);
        String userAgent = context.getString(R.string.device_info_user_agent, 
                brand, 
                getVersionNumber(context),
                Build.MODEL, 
                Build.VERSION.RELEASE);
        
        // NxWitness Android Client/[version] ([system and browser information]) [platform] ([platform details]) [extensions]

        // Application version: 2.3.1-dev.8529
        // Device model: Nexus 5
        // Firmware version: 5.1
        return userAgent;
    }
    
}
