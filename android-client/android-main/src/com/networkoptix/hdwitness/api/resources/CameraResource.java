package com.networkoptix.hdwitness.api.resources;

import java.util.UUID;

public class CameraResource extends AbstractResource {
    private static final long serialVersionUID = -3370871682332172818L;
    
    private static final String NO_VIDEO_SUPPORT = "noVideoSupport";
	
	private String mPhysicalId;
	
	public CameraResource(UUID id) {
		super(id, ResourceType.Camera);
	}

	public void setPhysicalId(String physicalId) {
	    mPhysicalId = physicalId;
	}
	
	public String getPhysicalId(){
		return mPhysicalId;
	}
	
	public boolean hasVideo() {
	    return !hasParam(NO_VIDEO_SUPPORT);
	}
	
	@Override
	public void update(AbstractResource resource) {
		if (mPhysicalId != getPhysicalId())
			throw new IllegalStateException();
//		mPhysicalId = getPhysicalId();
		super.update(resource);
	}
	
}
