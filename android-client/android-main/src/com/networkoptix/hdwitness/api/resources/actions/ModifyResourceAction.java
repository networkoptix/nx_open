package com.networkoptix.hdwitness.api.resources.actions;

import java.util.List;
import java.util.UUID;

import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.AbstractResource.ResourceType;
import com.networkoptix.hdwitness.api.resources.ResourcesList;

public abstract class ModifyResourceAction extends AbstractResourceAction {

	protected final UUID mId;
	private boolean mApplied = false;

	public ModifyResourceAction(UUID id) {
		mId = id;
	}

	@Override
	public void applyToResources(ResourcesList resources) {
        for (ResourceType rt: resources.getResourceTypes()) {
            mApplied = updateInList(resources.getResources(rt));
            if (mApplied)
                return;
        }
	}
	
	private boolean updateInList(List<AbstractResource> list){
	    AbstractResource resource = AbstractResource.findResourceById(list, mId);
	    if (resource == null)
	        return false;
	    doUpdate(resource);
	    return true;
	}

	protected abstract void doUpdate(AbstractResource resource);

}