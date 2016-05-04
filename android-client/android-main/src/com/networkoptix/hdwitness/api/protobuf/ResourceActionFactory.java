package com.networkoptix.hdwitness.api.protobuf;

import com.google.protobuf.implementation.MessageProtos.CameraServerItemMessage;
import com.google.protobuf.implementation.MessageProtos.Message;
import com.google.protobuf.implementation.MessageProtos.ResourceMessage;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.actions.AbstractResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.AddCameraHistoryItemAction;
import com.networkoptix.hdwitness.api.resources.actions.DeleteResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.ModifyResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.StatusResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.UpdateResourceAction;
import com.networkoptix.hdwitness.common.Utils;

public class ResourceActionFactory {
  
    public static AbstractResourceAction createAction(Message message) {

        switch (message.getType()) {
            case CameraServerItem: {
                final CameraServerItemMessage rm = message.getExtension(CameraServerItemMessage.message);
                return new AddCameraHistoryItemAction(
                        Utils.uuid(rm.getCameraServerItem().getServerGuid()), 
                        rm.getCameraServerItem().getPhysicalId(),
                        rm.getCameraServerItem().getTimestamp());
            }
            case Initial:
                break;
            case License:
                break;
            case Ping:
                break;
            case ResourceChange: {              
                final ResourceMessage rm = message.getExtension(ResourceMessage.message);
                return new UpdateResourceAction(ResourceFactory.createResource(rm.getResource()));
            }
            case ResourceDelete: {
                final ResourceMessage rm = message.getExtension(ResourceMessage.message);
                return new DeleteResourceAction(
                        ResourceFactory.idToUuid(rm.getResource().getId()));
            }
            case ResourceDisabledChange: {
                final ResourceMessage rm = message.getExtension(ResourceMessage.message);
                return new ModifyResourceAction(ResourceFactory.idToUuid(rm.getResource().getId())) {                    
                    @Override
                    protected void doUpdate(AbstractResource resource) {
                        resource.setDisabled(rm.getResource().getDisabled());
                    }
                };
            }
            case ResourceStatusChange: {
                final ResourceMessage rm = message.getExtension(ResourceMessage.message);
                return new StatusResourceAction(
                        ResourceFactory.idToUuid(rm.getResource().getId()),
                        ResourceFactory.resourceStatus(rm.getResource().getStatus())
                        );
            }
        default:
            break;
        }

        return null;
    }
    
}
