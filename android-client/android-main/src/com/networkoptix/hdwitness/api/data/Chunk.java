package com.networkoptix.hdwitness.api.data;

import com.networkoptix.hdwitness.BuildConfig;

import android.text.format.DateFormat;

/**
 * Time period described by its start and end time (in seconds since epoch)
 */
public final class Chunk {

    public final int start;
    public final int end;

    public Chunk(int _start, int length) {
        start = _start;
        end = start + length;
    }

    public boolean contains(int time) {
        return (time >= start) && (time < end);
    }

    @Override
    public String toString() {
        int length = (end - start);
        if (BuildConfig.DEBUG && length <= 0)
            throw new AssertionError();
        return "Chunk " + DateFormat.format("dd.mm kk:mm:ss", 1000L * start).toString() + "-" + DateFormat.format("kk:mm:ss", 1000L * end).toString();
    }

}