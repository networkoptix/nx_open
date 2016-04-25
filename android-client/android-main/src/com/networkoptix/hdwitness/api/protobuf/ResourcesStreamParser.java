package com.networkoptix.hdwitness.api.protobuf;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.implementation.CameraServerItemProtos.CameraServerItem;
import com.google.protobuf.implementation.CameraServerItemProtos.CameraServerItems;
import com.google.protobuf.implementation.MessageProtos;
import com.google.protobuf.implementation.CameraProtos.Camera;
import com.google.protobuf.implementation.LayoutProtos.Layout;
import com.google.protobuf.implementation.MessageProtos.CameraServerItemMessage;
import com.google.protobuf.implementation.MessageProtos.ResourceMessage;
import com.google.protobuf.implementation.ResourceProtos.Resource;
import com.google.protobuf.implementation.ResourceProtos.Resources;
import com.google.protobuf.implementation.ServerProtos.Server;
import com.google.protobuf.implementation.UserProtos.User;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.CameraHistory;
import com.networkoptix.hdwitness.api.resources.ResourcesList;
import com.networkoptix.hdwitness.api.resources.actions.AbstractResourceAction;
import com.networkoptix.hdwitness.common.Utils;

public class ResourcesStreamParser {

    private ExtensionRegistry registry = ExtensionRegistry.newInstance();
    private InputStream mStream;

    public ResourcesStreamParser(InputStream stream) {

        registry.add(ResourceMessage.message);
        registry.add(CameraServerItemMessage.message);
        registry.add(Camera.resource);
        registry.add(Server.resource);
        registry.add(User.resource);
        registry.add(Layout.resource);

        mStream = stream;
    }

    public AbstractResourceAction nextAction() {
        try {
            byte[] packetSize = new byte[4];

            mStream.read(packetSize);
            ByteBuffer buf = ByteBuffer.wrap(packetSize);
            int size = buf.getInt();

            byte[] data = new byte[size];
            int bytesRead = 0;

            do {
                int bytes = mStream.read(data, bytesRead, size - bytesRead);
                bytesRead += bytes;
            } while (bytesRead < size);

            MessageProtos.Message message = MessageProtos.Message.parseFrom(
                    data, registry);

            return ResourceActionFactory.createAction(message);
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    public ResourcesList resourcesList() {
        try {
            ResourcesList result = new ResourcesList();

            Resources list = Resources.parseFrom(mStream, registry);
            for (Resource r : list.getResourceList()) {
                AbstractResource resource = ResourceFactory.createResource(r);
                if (resource != null)
                    result.add(resource);
            }
            return result;
        } catch (IOException e) {
            e.printStackTrace();
            return null;            
        }
    }

    public CameraHistory cameraHistory() {
        try {
            CameraHistory result = new CameraHistory();

            CameraServerItems list = CameraServerItems.parseFrom(mStream, registry);
            for (CameraServerItem item : list.getCameraServerItemList()) {
                if (item.hasServerGuid() && item.hasPhysicalId() && item.hasTimestamp())
                    result.addItem(Utils.uuid(item.getServerGuid()), item.getPhysicalId(), item.getTimestamp());
            }
            return result;
        } catch (IOException e) {
            e.printStackTrace();
            return null;            
        }
    }

}
