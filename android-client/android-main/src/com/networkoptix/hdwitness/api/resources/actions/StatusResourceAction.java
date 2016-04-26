package com.networkoptix.hdwitness.api.resources.actions;

import java.util.UUID;

import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.Status;

public class StatusResourceAction extends ModifyResourceAction {

	private final Status mStatus;
	
	public StatusResourceAction(UUID id, Status status) {
		super(id);
		mStatus = status;
	}

	@Override
	protected void doUpdate(AbstractResource resource) {
	    resource.setStatus(mStatus);	    
	}
	
	@Override
	public boolean structureModified() {
	    return true;
	}
	
}
