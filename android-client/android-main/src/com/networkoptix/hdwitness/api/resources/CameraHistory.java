package com.networkoptix.hdwitness.api.resources;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.data.CameraChunkList;
import com.networkoptix.hdwitness.api.data.CameraHistoryChunk;

public class CameraHistory {
    
    Map<String, CameraChunkList> mChunks = new HashMap<String, CameraChunkList>();
    
    /**
     * Add new item to the history. Re-sorting is NOT applied.
     * @param serverGuid        - Guid of the server camera was moved to. 
     * @param cameraPhysicalId  - Physical Id of the camera.
     * @param timestamp         - Timestamp in milliseconds since epoch.
     */
    public void addItem(UUID serverGuid, String cameraPhysicalId, long timestampMs) {      
        if (!mChunks.containsKey(cameraPhysicalId))
            mChunks.put(cameraPhysicalId, new CameraChunkList());
        mChunks.get(cameraPhysicalId).add(new CameraHistoryChunk(serverGuid, timestampMs));       
    }

    public void clear() {
        mChunks.clear();
    }

    public void merge(CameraHistory other) {
        for(String physicalId: other.mChunks.keySet()) {
            if (!mChunks.containsKey(physicalId))
                mChunks.put(physicalId, other.mChunks.get(physicalId));
            else
                mChunks.get(physicalId).merge(other.mChunks.get(physicalId));
        }        
    }

    public Iterable<UUID> getAllServers(String cameraPhysicalId) {
        ArrayList<UUID> result = new ArrayList<UUID>();
        if (!mChunks.containsKey(cameraPhysicalId))
            return result;
        for(CameraHistoryChunk chunk: mChunks.get(cameraPhysicalId))
            if (!result.contains(chunk.serverGuid))
                result.add(chunk.serverGuid);
        return result;
    }

    public UUID getServerByTime(String cameraPhysicalId, long positionMs) {
        Log("looking for server by camera and time" + cameraPhysicalId  +" at " + positionMs);
        if (!mChunks.containsKey(cameraPhysicalId))
            return null;
        CameraChunkList list = mChunks.get(cameraPhysicalId);
        if (list.isEmpty())
            return null;
        
        for (int i = 0; i < list.size() - 1; i++) {
            if (list.get(i + 1).startMsec >= positionMs) {
                Log("chunk after " + i  +" has start later " + list.get(i + 1).startMsec + " so selecting "+ list.get(i).serverGuid);
                return list.get(i).serverGuid;
            }
        }
        
        Log("all moves were long ago, returning " + list.get(list.size() - 1).serverGuid);
        return list.get(list.size() - 1).serverGuid;
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }
    
}
