package com.networkoptix.hdwitness.api.resources.actions;

import com.networkoptix.hdwitness.api.resources.AbstractResource;

public class UpdateResourceAction extends ModifyResourceAction {

    private final AbstractResource mResource;
    private boolean mStructureModified = false;

    public UpdateResourceAction(AbstractResource resource) {
        super(resource.getId());
        mResource = resource;
    }
    
    @Override
    protected void doUpdate(AbstractResource resource) {
        mStructureModified = resource.getParentId() != mResource.getParentId();
        resource.update(mResource);
    }
    
    @Override
    public boolean structureModified() {
        return mStructureModified;
    }

}
