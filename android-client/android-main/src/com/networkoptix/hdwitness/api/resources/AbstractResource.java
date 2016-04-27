package com.networkoptix.hdwitness.api.resources;

import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

import android.net.Uri;

public class AbstractResource implements Serializable {
	private static final long serialVersionUID = -590809849776076498L;	

	public enum ResourceType {
	    Undefined,
	    Camera,
	    Server,
	    User,
	    Layout
	}
	
	private final UUID mId;
    private UUID mParentId;
    
	private String mName;
	private String mDisplayName;
	private Status mStatus = Status.Invalid;
	private String mUrl;
	private boolean mDisabled = false;
	
	private final ResourceType mResourceType;
	
	private Map<String, String> mParams = new HashMap<String, String>();

	public AbstractResource(UUID id, ResourceType resourceType) {
	    mId = id;
	    mResourceType = resourceType;
	}

	/**
	 * @return the id
	 */
	public UUID getId() {
		return mId;
	}
	
	public String getCompatibleId() {
	    return Uri.encode("{" + mId.toString() + "}");
	}
	
	public ResourceType getResourceType() {
	    return mResourceType;
	}

    public void setName(String name) {
        mName = name;
        
        /* Initial fill. */
        if (mDisplayName == null || mDisplayName.length() == 0)
            mDisplayName = name;
    }
	
	/**
	 * @return the name
	 */
	public String getName() {
	    if (mDisplayName == null || mDisplayName.length() == 0)
	        return mName;
	    return mDisplayName;
	}
	
	public void setDisplayName(String name) {
	    mDisplayName = name;
	}


    public void setParentId(UUID parentId) {
        mParentId = parentId;
    }
	
	/**
	 * @return the parentId
	 */
	public UUID getParentId() {
		return mParentId;
	}

	/**
	 * @return the status
	 */
	public Status getStatus() {
		return mStatus;
	}
	
	public void setStatus(Status status) {
	    if (status != Status.Invalid)
	        mStatus = status;
	}

	public void setUrl(String url) {
	    mUrl = url;
	}
	
	/**
	 * @return the url
	 */
	public String getUrl() {
		return mUrl;
	}
	
	public boolean isDisabled(){
		return mDisabled;
	}
	
	public void update(AbstractResource resource) {
		mName = resource.getName();
		mParentId = resource.getParentId();
		setStatus(resource.getStatus());
		mUrl = resource.getUrl();
		mDisabled = resource.isDisabled();
		mParams = resource.mParams;
	}
	
	public void setDisabled(boolean disabled){
		mDisabled = disabled;
	}

	public void addParam(String name, String value) {
	    mParams.put(name, value);
	}
	
	public boolean hasParam(String name) {
	    return mParams.containsKey(name);
	}
	
	public String getParam(String name, String defaultValue) {
	    if (mParams.containsKey(name))
	        return mParams.get(name);
	    return defaultValue;
	}
	
	public String getParam(String name) {
	    return mParams.get(name);
	}
	
    public static <T extends AbstractResource> T findResourceById(Iterable<T> list, UUID id) {
	    if (id == null)
	        return null;
	    
		for (T resource: list)
			if (resource.getId().equals(id))
				return resource;
		return null;
	}

	
}
