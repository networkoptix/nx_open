package com.networkoptix.hdwitness.api.ec2;

import java.util.Arrays;
import java.util.List;
import java.util.UUID;

import com.google.gson.JsonElement;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.AbstractResource.ResourceType;
import com.networkoptix.hdwitness.api.resources.CameraResource;
import com.networkoptix.hdwitness.api.resources.LayoutResource;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.UserResource;
import com.networkoptix.hdwitness.common.Utils;

public final class ApiWrappers {

    public final class TransactionWrapper {
        int command;
        JsonElement params;
    }
    
    public final class CustomTransactionWrapper {
        TransactionWrapper tran;
    }
    
    public class IdWrapper {
        String id;
        
        public UUID uuid() {
            return Utils.uuid(id);
        }
    }

    public final class ResourceParamWrapper {
        String name;
        String value;
        String resourceId;
        
        public UUID uuid() {
            return Utils.uuid(resourceId);
        }
    }
    
    public class ResourceWrapper extends IdWrapper {       
        String parentId;
        String status;
        String name;
        String url;
        List<ResourceParamWrapper> addParams;

        public void fill(AbstractResource resource) {
            if (status != null)
                resource.setStatus(Status.fromString(status));
            if (name != null)
                resource.setName(name);
            resource.setUrl(url);
            resource.setParentId(Utils.uuid(parentId));
            if (addParams != null)
                for (ResourceParamWrapper param: addParams) 
                    resource.addParam(param.name, param.value);
        }
        
        AbstractResource toResource() {
            AbstractResource resource = new AbstractResource(uuid(), ResourceType.Undefined);
            fill(resource);
            return resource;
        }
    }
    
    public final class ResourceStatusWrapper extends IdWrapper {
        public String status;
    }

    public final class UserResourceWrapper extends ResourceWrapper {

        boolean isAdmin;
        long permissions;
        String realm;

        UserResource toResource() {
            UserResource user = new UserResource(uuid());
            super.fill(user);
            user.setAdmin(isAdmin);
            user.setPermissions(permissions);
            user.setRealm(realm);

            return user;
        }
    }

    public final class ServerResourceWrapper extends ResourceWrapper {
        String networkAddresses;

        ServerResource toResource() {
            ServerResource server = new ServerResource(uuid());
            super.fill(server);
            server.setUncheckedNetAddrs(Arrays.asList(networkAddresses
                    .split(";")));

            return server;
        }
    }

    public final class LayoutResourceWrapper extends ResourceWrapper {

        public final class LayoutItemWrapper {
            String resourceId;
        }
        
        List<LayoutItemWrapper> items;

        LayoutResource toResource() {
            LayoutResource layout = new LayoutResource(uuid());
            super.fill(layout);

            for (LayoutItemWrapper item : items) {
                layout.getCameraIdsList().add(Utils.uuid(item.resourceId));
            }

            return layout;
        }
    }

    public final class CameraResourceWrapper extends ResourceWrapper {

        String physicalId;
        String model;

        CameraResource toResource() {          
            CameraResource camera = new CameraResource(uuid());
            super.fill(camera);
            camera.setPhysicalId(physicalId);

            return camera;
        }
    }
    
    
    public final class ResourceParamsWrapper extends IdWrapper {      
        List<ResourceParamWrapper> params;
    }

    
    public final class CameraHistoryItemWrapper {
        String cameraUniqueId;
        String serverGuid;
        long timestamp;
    }
    
    public final class CameraUserAttributesWrapper {
        String cameraID;
        String cameraName;
    }
    
    public final class ServerUserAttributesWrapper {
        String serverID;
        String serverName;
    }
}
