package com.networkoptix.hdwitness.api.data;

import java.io.Serializable;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.common.Utils;

public final class ChunkList implements Serializable {
    private static final long serialVersionUID = -8150529623654241692L;

    private static final long SPLIT_SIZE = 1000 * 60 * 60l; // 1 hour
    public static final long LAST_CUT_SIZE = 1000 * 60 * 2l; // 2 minutes
    public static final long CHUNK_DETAIL = 1000 * 60 * 1l; // 1 minutes

    private static final int INITIAL_CHUNK_SIZE = 256;

    private int[] mChunkStarts = new int[INITIAL_CHUNK_SIZE];
    private int[] mChunkLenghts = new int[INITIAL_CHUNK_SIZE];
    private int mSize = 0;

    public ChunkList() {
        super();
    }

    public ChunkList(long[][] source) {
        super();
        for (long[] chunk : source) {
            long start = chunk[0];
            long length = chunk[1];
            if (length < 0) {
                long end = System.currentTimeMillis() - LAST_CUT_SIZE;
                length = end - start;
            }
            
            while (length > SPLIT_SIZE + CHUNK_DETAIL) {
                addMSecs(start, SPLIT_SIZE);
                length -= SPLIT_SIZE;
                start += SPLIT_SIZE;
            }
            if (length > 0)
                addMSecs(start, length);
            
        }
        // Log("Chunks parsed:");
        // Log(toString());
    }

    public int startByIdx(int index) {
        return mChunkStarts[index];
    }

    public int endByIdx(int index) {
        return mChunkStarts[index] + mChunkLenghts[index];
    }

    public int lengthByIdx(int index) {
        return mChunkLenghts[index];
    }

    /**
     * Get the latest chunk that ends before the current starts.
     * 
     * @param current
     * @return
     */
    public Chunk previousOf(Chunk current) {
        if (mSize == 0)
            return null;

        int idx = (current == null) ? mSize : Utils.binarySearch(mChunkStarts, 0, mSize, current.start);
        if (idx < 0) // binary search algorithm
            idx = ~idx;
        if (idx > 0)
            idx -= 1;

        return chunk(idx);
    }

    /**
     * 
     * @param current
     * @return
     */
    public Chunk nextOf(Chunk current) {
        if (current == null || mSize == 0)
            return null;

        int idx = Utils.binarySearch(mChunkStarts, 0, mSize, current.start);
        if (idx < 0) // binary search algorithm
            idx = ~idx;
        idx += 1;

        if (idx >= mSize)
            return null;
        return chunk(idx);
    }

    public void merge(ChunkList other) {
        if (other.mSize == 0)
            return;

        if (mSize == 0) {
            mChunkStarts = Utils.copyOf(other.mChunkStarts, other.mChunkStarts.length);
            mChunkLenghts = Utils.copyOf(other.mChunkLenghts, other.mChunkLenghts.length);
            mSize = other.mSize;
            return;
        }

        // Log("Ready to merge:");
        // Log("this:");
        // Log(toString());
        // Log("other:");
        // Log(other.toString());

        int mergedSize = mSize + other.mSize;
        int[] mergedStarts = new int[mergedSize];
        int[] mergedLength = new int[mergedSize];

        int left = 0;
        int right = 0;
        int idx = 0;
        while (left < mSize || right < other.mSize) {

            int start = 0;
            int length = -1;

            if (left < mSize && right < other.mSize) {
                if (mChunkStarts[left] < other.mChunkStarts[right]) {
                    start = mChunkStarts[left];
                    length = mChunkLenghts[left];
                    left = left + 1;
                } else {
                    start = other.mChunkStarts[right];
                    length = other.mChunkLenghts[right];
                    right = right + 1;
                }
            } else if (left < mSize) {
                start = mChunkStarts[left];
                length = mChunkLenghts[left];
                left = left + 1;
            } else /* if (right < other.mSize) */{
                start = other.mChunkStarts[right];
                length = other.mChunkLenghts[right];
                right = right + 1;
            }

            /* Check if there is an intersection with a previous chunk. */
            if (idx > 0) {
                int prevEnd = mergedStarts[idx - 1] + mergedLength[idx - 1];
                if (start < prevEnd) {
                    int end = start + length;
                    /* Shifting start. */
                    start = prevEnd + 1;
                    length = end - start;
                }
            }

            if (length <= 0)
                continue;

            mergedStarts[idx] = start;
            mergedLength[idx] = length;
            idx = idx + 1;
        }

        mSize = idx;
        mChunkStarts = mergedStarts;
        mChunkLenghts = mergedLength;

        // Log("Merged successfully:");
        // Log(toString());
    }

    public int size() {
        return mSize;
    }

    public Chunk nearest(int position) {
        if (mSize == 0)
            return null;

        if (position < mChunkStarts[0])
            return chunk(0);

        if (position >= mChunkStarts[mSize - 1])
            return chunk(mSize - 1);

        int idx = Utils.binarySearch(mChunkStarts, 0, mSize, position);
        if (idx < 0) // binary search algorithm
            idx = ~idx;

        if (idx > 0 && mChunkStarts[idx - 1] <= position && mChunkStarts[idx - 1] + mChunkLenghts[idx - 1] >= position)
            return chunk(idx - 1);

        if (idx <= 0 || idx >= mSize - 1)
            return chunk(idx);

        int prevDiff = position - (mChunkStarts[idx - 1] + mChunkLenghts[idx - 1]);
        int nextDiff = mChunkStarts[idx + 1] - position;

        if (prevDiff < nextDiff)
            return chunk(idx - 1);
        else
            return chunk(idx + 1);
    }

    private Chunk chunk(int idx) {
        if (idx < 0 || idx >= mSize)
            return null;
        return new Chunk(mChunkStarts[idx], mChunkLenghts[idx]);
    }

    private void addMSecs(long startMSecs, long lengthMSecs) {
        int start = (int) (startMSecs / 1000);
        int length = (int) (lengthMSecs / 1000);
        add(start, length);
    }

    public void add(int start, int length) {
        if (mSize >= mChunkStarts.length)
            extendArrays();
        mChunkStarts[mSize] = start;
        mChunkLenghts[mSize] = length;
        mSize += 1;
    }

    private void extendArrays() {
        mChunkStarts = Utils.copyOf(mChunkStarts, mChunkStarts.length * 2);
        mChunkLenghts = Utils.copyOf(mChunkLenghts, mChunkLenghts.length * 2);
    }

    @SuppressWarnings("unused")
    private void Log(String message) {
        if (BuildConfig.DEBUG)
            Log.d("ChunkList", message);
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        result.append("Size: ");
        result.append(mSize);
        result.append('\n');
        for (int i = 0; i < mSize; i++) {
            result.append(chunk(i).toString());
            result.append('\n');
        }
        return result.toString();
    }
}
