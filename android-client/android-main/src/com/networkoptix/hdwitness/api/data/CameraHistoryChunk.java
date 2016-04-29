package com.networkoptix.hdwitness.api.data;

import java.util.UUID;

public final class CameraHistoryChunk {
    
    public final UUID serverGuid;
    public final long startMsec;
    
    public CameraHistoryChunk(UUID _serverGuid, long _startMsec) {
        serverGuid = _serverGuid;
        startMsec = _startMsec;
    }
}