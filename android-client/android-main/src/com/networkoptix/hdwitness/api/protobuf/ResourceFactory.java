package com.networkoptix.hdwitness.api.protobuf;

import java.util.Arrays;
import java.util.Iterator;
import java.util.UUID;

import com.google.protobuf.implementation.CameraProtos.Camera;
import com.google.protobuf.implementation.LayoutProtos.Layout;
import com.google.protobuf.implementation.LayoutProtos.Layout.Item;
import com.google.protobuf.implementation.ResourceProtos.Resource;
import com.google.protobuf.implementation.ServerProtos.Server;
import com.google.protobuf.implementation.UserProtos.User;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.CameraResource;
import com.networkoptix.hdwitness.api.resources.LayoutResource;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.UserResource;

public class ResourceFactory {

    /**
     * Compatibility function converting old ids to new.
     * @param id        Old-fashioned int id.
     * @return          New UUID.
     */
    public static UUID idToUuid(int id) {
        return new UUID(0, id);
    }
    
    public static Status resourceStatus(
            com.google.protobuf.implementation.ResourceProtos.Resource.Status status) {
        switch (status) {
        case Offline:
            return com.networkoptix.hdwitness.api.resources.Status.Offline;
        case Online:
            return com.networkoptix.hdwitness.api.resources.Status.Online;
        case Recording:
            return com.networkoptix.hdwitness.api.resources.Status.Recording;
        case Unauthorized:
            return com.networkoptix.hdwitness.api.resources.Status.Unauthorized;
        default:
            break;
        }
        return com.networkoptix.hdwitness.api.resources.Status.Offline;
    }

    public static AbstractResource createResource(Resource resource) {
        
        AbstractResource result = null;
        
        switch (resource.getType()) {
        case Camera:
            result = createCameraResource(resource);
            break;
        case Layout:
            result = createLayoutResource(resource);
            break;
        case Server:
            result = createServerResource(resource);
            break;
        case User:
            result = createUserResource(resource);
            break;
        default:
            break;
        }
        if (result == null)
            return null;
        
        result.setName(resource.getName());
        result.setParentId(resource.hasParentId() ? idToUuid(resource.getParentId()) : null);
        result.setStatus(resource.hasStatus() ? resourceStatus(resource.getStatus()) : Status.Online);
        result.setUrl(resource.hasUrl() ? resource.getUrl() : null);
        result.setDisabled(resource.hasDisabled() && resource.getDisabled());
        
        return result;
    }

    private static AbstractResource createCameraResource(Resource resource) {
        if (!resource.hasExtension(Camera.resource))
            return null;    
        Camera extension = resource.getExtension(Camera.resource);
        
        CameraResource camera = new CameraResource(idToUuid(resource.getId()));
        camera.setPhysicalId(extension.getPhysicalId());
        
        return camera;
    }

    private static AbstractResource createUserResource(Resource resource) {
        if (!resource.hasExtension(User.resource))
            return null;
        User extension = resource.getExtension(User.resource);
        
        UserResource user = new UserResource(idToUuid(resource.getId()));
        user.setAdmin(extension.hasIsAdmin() && extension.getIsAdmin());
        user.setPermissions(extension.hasRights() ? extension.getRights() : 0);
        
        return user;
    }

    private static ServerResource createServerResource(Resource resource) {
        if (!resource.hasExtension(Server.resource))
            return null;
        
        Server extension = resource.getExtension(Server.resource);
        
        ServerResource server = new ServerResource(idToUuid(resource.getId()));
        if (extension.hasNetAddrList())
            server.setUncheckedNetAddrs(Arrays.asList(extension.getNetAddrList().split(";")));
        return server;
    }

    private static LayoutResource createLayoutResource(Resource resource) {
        if (!resource.hasExtension(Layout.resource))
            return null;
        
        Layout extension = resource.getExtension(Layout.resource);
        LayoutResource layout = new LayoutResource(idToUuid(resource.getId()));
           
            Iterator<Item> iter = extension.getItemList().iterator();
            while (iter.hasNext()){
                Item it = iter.next();
                if (it.hasResource() && it.getResource().hasId())
                    layout.getCameraIdsList().add(idToUuid(it.getResource().getId()));
            }
   
        return layout;
    }

}
