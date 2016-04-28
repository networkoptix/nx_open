package com.networkoptix.hdwitness.api.data;

import java.util.ArrayList;

import com.networkoptix.hdwitness.BuildConfig;

public final class CameraChunkList extends ArrayList<CameraHistoryChunk> {
    private static final long serialVersionUID = 3882555951828122818L;

    public void merge(CameraChunkList other) {           
        if (other.isEmpty())
            return;
        
        if (isEmpty()) {
            addAll(other);
            return;
        }
                  
        //merge sort
        int i = 0;
        int j = 0;
        while (i < size() && j < other.size()) {
            if (this.get(i).startMsec < other.get(j).startMsec) {
                i++;
            } else if (this.get(i).startMsec == other.get(j).startMsec) {
                i++;
                j++;    //we don't want same elements
            }
            else {
                this.add(i, other.get(j));
                i++;
                j++;
            }               
        }
        while (j < other.size()) {
            this.add(other.get(j));
            j++;
        }
        
        if (BuildConfig.DEBUG && !isSorted())
            throw new AssertionError();
    }

    private boolean isSorted() {
        long prev = 0;
        for (CameraHistoryChunk chunk: this) {
            if (chunk.startMsec <= prev)
                return false;
            prev = chunk.startMsec;
        }
        return true;
    }
    
}