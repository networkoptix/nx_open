package com.networkoptix.hdwitness.api.resources.actions;

import com.networkoptix.hdwitness.api.resources.CameraHistory;
import com.networkoptix.hdwitness.api.resources.ResourcesList;

public abstract class AbstractResourceAction {

	public void applyToResources(ResourcesList resources) {}
	public void applyToCameraHistory(CameraHistory history) {}

    public boolean structureModified() {
        return false;
    }

}
