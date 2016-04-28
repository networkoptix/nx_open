package com.networkoptix.hdwitness.api.resources;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.resources.AbstractResource.ResourceType;

public class ResourcesList implements Serializable {

    private static final long serialVersionUID = -3985003115156990944L;

    private ArrayList<ResourceType> mResourceTypes = new ArrayList<AbstractResource.ResourceType>();
    private Map<ResourceType, ArrayList<AbstractResource>> mResources = new HashMap<AbstractResource.ResourceType, ArrayList<AbstractResource>>();

    public ResourcesList() {
        mResourceTypes.add(ResourceType.Camera);
        mResourceTypes.add(ResourceType.User);
        mResourceTypes.add(ResourceType.Server);
        mResourceTypes.add(ResourceType.Layout);

        for (ResourceType rt : mResourceTypes)
            mResources.put(rt, new ArrayList<AbstractResource>());
    }

    public Iterable<ResourceType> getResourceTypes() {
        return mResourceTypes;
    }

    public List<AbstractResource> getResources(ResourceType resourceType) {
        return mResources.get(resourceType);
    }

    public void add(AbstractResource resource) {
        if (BuildConfig.DEBUG && resource.getResourceType() == ResourceType.Undefined)
            throw new AssertionError();
        mResources.get(resource.getResourceType()).add(resource);
    }

    public void clear() {
        for (ResourceType rt : mResourceTypes)
            mResources.get(rt).clear();
    }

    public void merge(ResourcesList other) {
        for (ResourceType rt : mResourceTypes)
            mergeResourcesListInArray(mResources.get(rt), other.mResources.get(rt));
    }

    private void mergeResourcesListInArray(ArrayList<AbstractResource> list, ArrayList<AbstractResource> updated) {
        for (AbstractResource resource : updated) {
            AbstractResource existing = AbstractResource.findResourceById(list, resource.getId());
            if (existing != null)
                existing.update(resource);
            else
                list.add(resource);
        }
    }


    public AbstractResource findById(UUID id) {
        for (ResourceType rt : mResourceTypes) {
            AbstractResource result = AbstractResource.findResourceById(mResources.get(rt), id);
            if (result != null)
                return result;
        }
        return null;
    }

    public AbstractResource findById(UUID id, ResourceType rt) {
        return AbstractResource.findResourceById(mResources.get(rt), id);
    }

}
