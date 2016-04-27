package com.networkoptix.hdwitness.common;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;

public class SoftwareVersion implements Comparable<SoftwareVersion> {

    private int mMajor = 0;
    private int mMinor = 0;
    private int mRelease = 0;
    private int mBuild = 0;
    
    private static final Pattern mVersionPattern = Pattern.compile("(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)");
    
    public SoftwareVersion() {
        
    }
    
    public SoftwareVersion(int major, int minor) {
        mMajor = major;
        mMinor = minor;
    }
    
    public SoftwareVersion(int major, int minor, int release) {
        mMajor = major;
        mMinor = minor;
        mRelease = release;
    }
    
    public SoftwareVersion(int major, int minor, int release, int build) {
        mMajor = major;
        mMinor = minor;
        mRelease = release;
        mBuild = build;
    }
    
    public SoftwareVersion(String source) {
        Log("Parsing software version " + source);
        if (source == null)
            return;
        
        Matcher matcher = mVersionPattern.matcher(source);
        if (!matcher.matches())
            return;

        mMajor = Integer.parseInt(matcher.group(1));
        mMinor = Integer.parseInt(matcher.group(2));
        mRelease = Integer.parseInt(matcher.group(3));
        mBuild = Integer.parseInt(matcher.group(4));
    }
    
    public boolean isValid() {
        return mMajor > 0;
    }
    
    public boolean lessThan(SoftwareVersion another) {
        return this.compareTo(another) < 0;
    }
    
    @Override
    public String toString() {
        char delimiter = '.';
        StringBuilder sb = new StringBuilder();
        sb
        .append(mMajor).append(delimiter)
        .append(mMinor).append(delimiter)
        .append(mRelease).append(delimiter)
        .append(mBuild);
        return sb.toString();
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof SoftwareVersion))
            return false;
        SoftwareVersion another = (SoftwareVersion)o;    
        
        return this.compareTo(another) == 0;
    }
    
    @Override
    public int compareTo(SoftwareVersion another) {
        int diff = mMajor - another.mMajor;
        if (diff != 0)
            return diff;
        diff = mMinor - another.mMinor;
        if (diff != 0)
            return diff;
        
        diff = mRelease - another.mRelease;
        if (diff != 0)
            return diff;
        
        return mBuild - another.mBuild;
    }
    
    private void Log(String message) {
        if (BuildConfig.DEBUG)
            Log.d(this.getClass().getSimpleName(), message);
    }
}
