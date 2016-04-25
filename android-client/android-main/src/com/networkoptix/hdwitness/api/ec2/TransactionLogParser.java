package com.networkoptix.hdwitness.api.ec2;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Type;
import java.util.List;

import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.CameraHistory;
import com.networkoptix.hdwitness.api.resources.ResourcesList;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.actions.AbstractResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.AddCameraHistoryItemAction;
import com.networkoptix.hdwitness.api.resources.actions.DeleteResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.ModifyResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.StatusResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.UpdateResourceAction;
import com.networkoptix.hdwitness.api.resources.actions.UpdateResourcesAction;
import com.networkoptix.hdwitness.common.SoftwareVersion;
import com.networkoptix.hdwitness.common.Utils;

public class TransactionLogParser {

    private static final int MULTICHUNK_PROTO_VERSION = 1014;
    private static final SoftwareVersion MULTICHUNK_VERSION = new SoftwareVersion(2, 3, 1);

    public interface HttpStreamReader {      
        String readTransaction() throws IOException;
    }
    
    private final HttpStreamReader mStreamReader;
    
    public TransactionLogParser(InputStream stream, SoftwareVersion serverVersion, int protocolVersion) {
        Log("Software version: " + serverVersion + "; protocol version: " + protocolVersion);
        if (serverVersion.lessThan(MULTICHUNK_VERSION) || 
                (serverVersion.equals(MULTICHUNK_VERSION) && protocolVersion < MULTICHUNK_PROTO_VERSION)) {
            Log("Using chunked http protocol");
            mStreamReader = new ChunkedHttpStreamReader(stream);
        }
        else {
            Log("Using multipart http protocol");
            mStreamReader = new MultipartHttpStreamReader(stream);
        }
    }

    public AbstractResourceAction nextAction() throws IOException {       
        final String json = mStreamReader.readTransaction();
        if (json == null) 
            throw new IOException("Connection aborted");

        ApiWrappers.CustomTransactionWrapper info = null;
        final Gson gson = new Gson();
        try {
            info = gson.fromJson(json, ApiWrappers.CustomTransactionWrapper.class);
        } catch (com.google.gson.JsonSyntaxException e) {
            Error("Malformed transaction: " + json);
            e.printStackTrace();
            return null;
        }
        
        if (info == null) {
            Error("Malformed transaction: " + json);
            return null;
        }

        String command = TransactionCommands.commandName(info.tran.command);
        Log("Received transaction " + command);
        switch (info.tran.command) {

        // -------------- modify list of resources --------------------
        case TransactionCommands.getMediaServersEx: {
            return createUpdateResourcesAction(ApiWrappers.ServerResourceWrapper.class,
                    new TypeToken<List<ApiWrappers.ServerResourceWrapper>>() {
                    }.getType(), info, gson);
        }

        case TransactionCommands.saveCameras:
        case TransactionCommands.getCamerasEx: {
            return createUpdateResourcesAction(ApiWrappers.CameraResourceWrapper.class,
                    new TypeToken<List<ApiWrappers.CameraResourceWrapper>>() {
                    }.getType(), info, gson);
        }

        case TransactionCommands.getUsers: {
            return createUpdateResourcesAction(ApiWrappers.UserResourceWrapper.class,
                    new TypeToken<List<ApiWrappers.UserResourceWrapper>>() {
                    }.getType(), info, gson);
        }

        case TransactionCommands.saveLayouts:
        case TransactionCommands.getLayouts: {
            return createUpdateResourcesAction(ApiWrappers.LayoutResourceWrapper.class,
                    new TypeToken<List<ApiWrappers.LayoutResourceWrapper>>() {
                    }.getType(), info, gson);
        }
        case TransactionCommands.saveResource: {
            ApiWrappers.ResourceWrapper wrapper = gson.fromJson(info.tran.params, ApiWrappers.ResourceWrapper.class);
            return new UpdateResourceAction(wrapper.toResource());
        }
        case TransactionCommands.removeResource:
        case TransactionCommands.removeCamera:
        case TransactionCommands.removeMediaServer:
        case TransactionCommands.removeUser:
        case TransactionCommands.removeLayout: {
            final ApiWrappers.IdWrapper wrapper = gson.fromJson(info.tran.params, ApiWrappers.IdWrapper.class);
            return new DeleteResourceAction(wrapper.uuid());
        }
        case TransactionCommands.saveCamera: {
            final ApiWrappers.CameraResourceWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.CameraResourceWrapper.class);
            if (wrapper.model.equals("virtual desktop camera")) 
                return null;
            return new UpdateResourceAction(wrapper.toResource());
        }
        case TransactionCommands.saveMediaServer: {
            final ApiWrappers.ServerResourceWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.ServerResourceWrapper.class);
            return new UpdateResourceAction(wrapper.toResource());
        }
        case TransactionCommands.saveUser: {
            final ApiWrappers.UserResourceWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.UserResourceWrapper.class);
            return new UpdateResourceAction(wrapper.toResource());
        }
        case TransactionCommands.saveLayout: {
            final ApiWrappers.LayoutResourceWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.LayoutResourceWrapper.class);
            return new UpdateResourceAction(wrapper.toResource());
        }

        // -------------- modify single resource ----------------------
        case TransactionCommands.setResourceStatus: {
            final ApiWrappers.ResourceStatusWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.ResourceStatusWrapper.class);
            return new StatusResourceAction(wrapper.uuid(), Status.fromString(wrapper.status));
        }
        case TransactionCommands.setResourceParams: {
            final ApiWrappers.ResourceParamsWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.ResourceParamsWrapper.class);
            return new ModifyResourceAction(wrapper.uuid()) {

                @Override
                protected void doUpdate(AbstractResource resource) {
                    for (ApiWrappers.ResourceParamWrapper param : wrapper.params)
                        resource.addParam(param.name, param.value);
                }
            };
        }
        case TransactionCommands.setResourceParam: {
            final ApiWrappers.ResourceParamWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.ResourceParamWrapper.class);
            return new ModifyResourceAction(wrapper.uuid()) {

                @Override
                protected void doUpdate(AbstractResource resource) {
                    Log("Updating resource " + resource.getName() + " param " + wrapper.name);
                    resource.addParam(wrapper.name, wrapper.value);
                }
            };
        }
        case TransactionCommands.saveCameraUserAttributes: {
            final ApiWrappers.CameraUserAttributesWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.CameraUserAttributesWrapper.class);
            return new ModifyResourceAction(Utils.uuid(wrapper.cameraID)) {

                @Override
                protected void doUpdate(AbstractResource resource) {
                    resource.setDisplayName(wrapper.cameraName);
                }
            };
        }
        case TransactionCommands.saveServerUserAttributes: {
            final ApiWrappers.ServerUserAttributesWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.ServerUserAttributesWrapper.class);
            return new ModifyResourceAction(Utils.uuid(wrapper.serverID)) {

                @Override
                protected void doUpdate(AbstractResource resource) {
                    resource.setDisplayName(wrapper.serverName);
                }
            };
        }

        // -------------- modify camera history -----------------------
        case TransactionCommands.getCameraHistoryItems: {
            final List<ApiWrappers.CameraHistoryItemWrapper> wrappers = gson.fromJson(info.tran.params,
                    new TypeToken<List<ApiWrappers.CameraHistoryItemWrapper>>() {
                    }.getType());

            return new AbstractResourceAction() {
                @Override
                public void applyToCameraHistory(CameraHistory history) {
                    for (ApiWrappers.CameraHistoryItemWrapper wrapper : wrappers)
                        history.addItem(Utils.uuid(wrapper.serverGuid), wrapper.cameraUniqueId, wrapper.timestamp);
                }
            };
        }
        case TransactionCommands.addCameraHistoryItem: {
            final ApiWrappers.CameraHistoryItemWrapper wrapper = gson.fromJson(info.tran.params,
                    ApiWrappers.CameraHistoryItemWrapper.class);
            return new AddCameraHistoryItemAction(Utils.uuid(wrapper.serverGuid), wrapper.cameraUniqueId, wrapper.timestamp);
        }
        }

        return null;
    }

    private <K extends ApiWrappers.ResourceWrapper> AbstractResourceAction createUpdateResourcesAction(
            Class<K> classOfK, Type typeOfKList, ApiWrappers.CustomTransactionWrapper info, Gson gson) {

        final List<K> wrappers = gson.fromJson(info.tran.params, typeOfKList);
        if (wrappers.isEmpty())
            return null;

        final ResourcesList resources = new ResourcesList();
        for (final K wrapper : wrappers)
            resources.add(wrapper.toResource());
        return new UpdateResourcesAction(resources);
    }
    
    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

    private void Error(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.e(this.getClass().getSimpleName(), message);
    }
    
}
