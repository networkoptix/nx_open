package com.networkoptix.hdwitness.api.resources;

import java.util.ArrayList;
import java.util.UUID;

public class LayoutResource extends AbstractResource {
    
    private static final long serialVersionUID = -2044333558351321295L;

	private ArrayList<UUID> mCameras = new ArrayList<UUID>();

    public LayoutResource(UUID id) {
        super(id, ResourceType.Layout);
    }	

	public ArrayList<UUID> getCameraIdsList() {
		return mCameras;
	}
	
	@Override
	public void update(AbstractResource resource) {
		if (!(resource instanceof LayoutResource))
			throw new IllegalStateException();
		LayoutResource layout = (LayoutResource)resource;
		
		mCameras.clear();
		mCameras.addAll(layout.getCameraIdsList());
		super.update(resource);
	}
	
}
