package com.networkoptix.hdwitness.api.resources.actions;

import java.util.UUID;

import com.networkoptix.hdwitness.api.resources.CameraHistory;

public class AddCameraHistoryItemAction extends AbstractResourceAction {

    private final UUID mServerGuid;
    private final String mCameraPhysicalId;
    private final long mTimestamp;

    public AddCameraHistoryItemAction(UUID serverGuid, String cameraPhysicalId, long timestamp) {
        mServerGuid = serverGuid;
        mCameraPhysicalId = cameraPhysicalId;
        mTimestamp = timestamp;
    }

    @Override
    public void applyToCameraHistory(CameraHistory history) {
        history.addItem(mServerGuid, mCameraPhysicalId, mTimestamp);
    }

}
