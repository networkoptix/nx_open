package com.networkoptix.hdwitness.api.resources.actions;

import java.util.List;
import java.util.UUID;

import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.ResourcesList;
import com.networkoptix.hdwitness.api.resources.AbstractResource.ResourceType;

public class DeleteResourceAction extends AbstractResourceAction {

    private final UUID mId;
    private boolean mApplied = false;

    public DeleteResourceAction(UUID id) {
        mId = id;
    }

    private boolean deleteResourcesListInArray(List<AbstractResource> list, UUID id) {
        AbstractResource existing = AbstractResource.findResourceById(list, id);
        if (existing != null) {
            list.remove(existing);
            return true;
        }
        return false;
    }

    @Override
    public void applyToResources(ResourcesList resources) {
        for (ResourceType rt: resources.getResourceTypes()) {
            mApplied = deleteResourcesListInArray(resources.getResources(rt), mId);
            if (mApplied)
                return;
        }
    }

    @Override
    public boolean structureModified() {
        return mApplied;
    }
}
