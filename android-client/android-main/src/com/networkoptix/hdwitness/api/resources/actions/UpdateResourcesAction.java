package com.networkoptix.hdwitness.api.resources.actions;

import com.networkoptix.hdwitness.api.resources.ResourcesList;

public class UpdateResourcesAction extends AbstractResourceAction {

    private final ResourcesList mResources;

    public UpdateResourcesAction(ResourcesList resources) {
        mResources = resources;
    }
    
    @Override
    public void applyToResources(ResourcesList resources) {
        resources.merge(mResources);
    }

    @Override
    public boolean structureModified() {
        return true;
    }
}
