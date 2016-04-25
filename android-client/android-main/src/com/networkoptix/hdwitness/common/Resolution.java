package com.networkoptix.hdwitness.common;

import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Resolution implements Comparable<Resolution> {

    private int mWidth = 0;
    private int mHeigth = 0;
    private boolean mValid = false;
    private boolean mNative = false;
    private int mStreamIndex = -1;

    private static final Pattern mResolutionPattern = Pattern.compile("(\\d+)x(\\d+)");

    @SuppressWarnings("serial")
    public static final ArrayList<Resolution> STANDARD_RESOLUTIONS = new ArrayList<Resolution>() {
        {
            add(new Resolution(240, 160));
            add(new Resolution(240));
            add(new Resolution(480, 320));
            add(new Resolution(360));
            add(new Resolution(480));
            add(new Resolution(720));
            add(new Resolution(1080));
        }
    };

    public Resolution(int height) {
        mHeigth = height;
        mValid = mHeigth > 0;
    }

    public Resolution(String encoded) {
        Matcher matcher = mResolutionPattern.matcher(encoded);
        mValid = matcher.matches();
        if (!mValid)
            return;
        mWidth = Integer.parseInt(matcher.group(1));
        mHeigth = Integer.parseInt(matcher.group(2));
        mValid = mHeigth > 0;
    }

    public Resolution(int width, int height) {
        mWidth = width;
        mHeigth = height;
        mValid = mHeigth > 0;
    }

    public String toRawString() {
        if (!mValid)
            return "";
        StringBuilder sb = new StringBuilder();
        if (mWidth == 0)
            sb.append(mHeigth).append("p");
        else
            sb.append(mWidth).append("x").append(mHeigth);
        return sb.toString();
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        if (mValid) {
            if (mWidth == 0)
                sb.append(mHeigth).append("p");
            else
                sb.append(mWidth).append("x").append(mHeigth);
        } else {
            sb.append("Unknown");
        }
        if (mNative)
            sb.append('*');
        return sb.toString();
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof Resolution))
            return false;
        Resolution other = (Resolution) o;
        return other.mWidth == mWidth &&
                other.mHeigth == mHeigth && 
                other.mNative == mNative &&
                other.mStreamIndex == mStreamIndex;
    }

    public boolean isNative() {
        return mNative;
    }

    public void setNative(boolean value) {
        mNative = value;
    }

    public int getStreamIndex() {
        return mStreamIndex;
    }

    public void setStreamIndex(int index) {
        mStreamIndex = index;
    }

    public int getWidth() {
        return mWidth;
    }
    
    public int getHeight() {
        return mHeigth;
    }

    public boolean isValid() {
        return mValid;
    }
    
    @Override
    public int compareTo(Resolution another) {
        int diff = mHeigth - another.mHeigth;

        if (diff != 0)
            return diff;

        return mNative == another.mNative ? 0 : mNative ? 1 : -1;
    }
}