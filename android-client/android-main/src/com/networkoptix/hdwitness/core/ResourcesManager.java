package com.networkoptix.hdwitness.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.UUID;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.AbstractResource.ResourceType;
import com.networkoptix.hdwitness.api.resources.CameraHistory;
import com.networkoptix.hdwitness.api.resources.CameraResource;
import com.networkoptix.hdwitness.api.resources.LayoutResource;
import com.networkoptix.hdwitness.api.resources.ResourcesList;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.actions.AbstractResourceAction;
import com.networkoptix.hdwitness.common.network.HostnameResolver;
import com.networkoptix.hdwitness.common.network.HostnameResolver.HostnameInfo;
import com.networkoptix.hdwitness.common.network.HostnameResolver.OnResolveListener;

public class ResourcesManager {
    public interface OnServerResolvedListener {
        void onServerConfirmed(ServerResource server);
    }

    /** full resources list */
    private ResourcesList mResources = new ResourcesList();

    private CameraHistory mCameraHistory = new CameraHistory();

    // resources tree structure
    private HashMap<UUID, ArrayList<CameraResource>> mCamerasByServer = new HashMap<UUID, ArrayList<CameraResource>>();

    private String mProxyHost;
    private int mProxyPort = 0;
    private UUID mProxyUuid;

    private HostnameResolver mHostnameResolver = new HostnameResolver();
    private OnServerResolvedListener mServerResolveListener;

    private boolean mResolvingEnabled = false;

    public ResourcesManager() {
        mHostnameResolver.setOnResolveListener(new OnResolveListener() {

            public void resolved(HostnameInfo info) {
                for (AbstractResource resource : mResources.getResources(ResourceType.Server)) {
                    ServerResource server = (ServerResource) resource;
                    if (!server.getId().equals(info.Guid))
                        continue;
                    server.confirmHostname(info.Hostname);
                    if (mServerResolveListener != null)
                        mServerResolveListener.onServerConfirmed(server);
                    return;
                }
            }

            public void disbanded(HostnameResolver.HostnameInfo info) {
                for (AbstractResource resource : mResources.getResources(ResourceType.Server)) {
                    ServerResource server = (ServerResource) resource;
                    if (!server.getId().equals(info.Guid))
                        continue;
                    server.disbandHostname(info.Hostname);
                    // if that was the last interface, notify others to force
                    // using proxy
                    if (server.isResolvedToProxy())
                        if (mServerResolveListener != null)
                            mServerResolveListener.onServerConfirmed(server);
                    return;
                }
            }
        });
    }

    public void update(ResourcesList resources) {
        if (resources != null)
            mResources.merge(resources);

        rebuildAll();
    }

    private void rebuildAll() {
        rebuildAdminTree();
        if (mResolvingEnabled)
            resolveServers();
    }

    public void setMediaProxy(String host, int port, UUID uuid) {
        mProxyHost = host;
        mProxyPort = port;
        mProxyUuid = uuid;
    }

    public void clear() {
        mHostnameResolver.clear();
        mResources.clear();
    }

    public Iterable<LayoutResource> getLayoutsByUserId(UUID userId) {
        ArrayList<LayoutResource> result = new ArrayList<LayoutResource>();
        if (userId == null)
            return result;

        for (AbstractResource layout : mResources.getResources(ResourceType.Layout)) {
            if (layout.getParentId().equals(userId))
                result.add((LayoutResource) layout);
        }

        return result;
    }

    public ServerResource getServerByCameraId(UUID cameraId) {
        AbstractResource camera = mResources.findById(cameraId, ResourceType.Camera);
        if (camera == null)
            return null;
        AbstractResource server = mResources.findById(camera.getParentId(), ResourceType.Server);
        if (server != null)
            return (ServerResource) server;
        return null;
    }

    public CameraResource getEnabledCameraById(UUID cameraId) {
        AbstractResource original = mResources.findById(cameraId, ResourceType.Camera);
        if (original == null || !original.isDisabled())
            return (CameraResource) original;

        String physicalId = ((CameraResource) original).getPhysicalId();

        for (AbstractResource resource : mResources.getResources(ResourceType.Camera)) {
            if (resource.isDisabled())
                continue;

            CameraResource camera = (CameraResource) resource;
            if (!camera.getPhysicalId().equals(physicalId))
                continue;
            return camera;
        }
        return null;
    }

    public Iterable<AbstractResource> getUsers() {
        return mResources.getResources(ResourceType.User);
    }

    public Iterable<AbstractResource> getAllServers() {
        return mResources.getResources(ResourceType.Server);
    }

    public Iterable<CameraResource> getCamerasByServerId(UUID serverId) {
        return mCamerasByServer.get(serverId);
    }

    private void rebuildAdminTree() {
        mCamerasByServer.clear();
        for (AbstractResource server : mResources.getResources(ResourceType.Server)) {
            if (!mCamerasByServer.containsKey(server.getId()))
                mCamerasByServer.put(server.getId(), new ArrayList<CameraResource>());
        }

        for (AbstractResource resource : mResources.getResources(ResourceType.Camera)) {
            CameraResource camera = (CameraResource) resource;
            if (camera.isDisabled())
                continue;
            
            if (!camera.hasVideo())
                continue;

            AbstractResource server = mResources.findById(camera.getParentId(), ResourceType.Server);
            if (server == null)
                continue;
            if (!mCamerasByServer.containsKey(server.getId())) {
                mCamerasByServer.put(server.getId(), new ArrayList<CameraResource>());
            }

            ArrayList<CameraResource> serverCameras = mCamerasByServer.get(server.getId());
            if (serverCameras.indexOf(camera) < 0)
                serverCameras.add(camera);
            if (server.getStatus() == Status.Offline)
                camera.setStatus(Status.Offline);
        }
    }

    private void resolveServers() {
        for (AbstractResource resource : mResources.getResources(ResourceType.Server)) {
            ServerResource server = (ServerResource) resource;
            if (server.getStatus() == Status.Online && !server.isResolved())
                resolveServer(server);
        }
    }

    private void resolveServer(ServerResource server) {
        ArrayList<String> hostnames = new ArrayList<String>();
        hostnames.addAll(server.uncheckedNetAddrs());
        for (String hostname : hostnames) {
            switch (mHostnameResolver.resolveHostname(hostname, server.getPort(), server.getId())) {
            case Disbanded:
                server.disbandHostname(hostname);
                continue;
            case InProgress:
                continue;
            case Resolved:
                server.confirmHostname(hostname);
                return;
            }
        }

    }

    public void applyAction(AbstractResourceAction action) {
        action.applyToResources(mResources);
        action.applyToCameraHistory(mCameraHistory);
        if (action.structureModified())
            rebuildAll();
    }

    public OnServerResolvedListener getOnServerResolverListener() {
        return mServerResolveListener;
    }

    public void setOnServerResolvedListener(OnServerResolvedListener listener) {
        mServerResolveListener = listener;
    }

    public CameraHistory getHistory() {
        return mCameraHistory;
    }

    private ServerResource findServerById(UUID serverGuid) {
        for (AbstractResource resource : mResources.getResources(ResourceType.Server)) {
            ServerResource server = (ServerResource) resource;
            if (server.getId().equals(serverGuid))
                return server;
        }
        return null;
    }

    public Iterable<ServerResource> getAllServersByCamera(CameraResource camera) {
        Log("looking list of server for camera " + camera.getName());
        ArrayList<ServerResource> result = new ArrayList<ServerResource>();
        AbstractResource currentServer = mResources.findById(camera.getParentId(), ResourceType.Server);
        if (currentServer != null)
            result.add((ServerResource) currentServer);

        for (UUID serverGuid : mCameraHistory.getAllServers(camera.getPhysicalId())) {
            ServerResource server = findServerById(serverGuid);
            if (server == null || result.contains(server))
                continue;
            result.add(server);
        }

        for (ServerResource server : result) {
            Log("found server " + server.getName());
        }

        return result;
    }

    public ServerResource getServerByCameraTime(CameraResource camera, long positionMs) {
        ServerResource currentServer = (ServerResource) mResources.findById(camera.getParentId(), ResourceType.Server);

        if (positionMs < 0)
            return currentServer;

        UUID serverGuid = mCameraHistory.getServerByTime(camera.getPhysicalId(), positionMs);
        Log("historical search found " + serverGuid + " while current server is " + currentServer);

        ServerResource server = findServerById(serverGuid);
        Log("historical search server " + server);
        if (server == null || server.getStatus() != Status.Online) // return
                                                                   // current
                                                                   // server in
                                                                   // case of
                                                                   // unavailability
            return currentServer;
        return server;
    }

    public void setResolvingEnabled(boolean enabled) {
        if (mResolvingEnabled == enabled)
            return;

        mResolvingEnabled = enabled;
        if (mResolvingEnabled)
            resolveServers();
    }

    public String getServerUrl(ServerResource targetServer, String protocol) {
        
        if (mResolvingEnabled && targetServer != null) {
            String directUrl = targetServer.getHostname() + ":" + targetServer.getPort() + "/";
            if (targetServer.isResolved())
                return protocol + "://" + directUrl;

//            if (!targetServer.isResolvedToProxy())
//                Log("Requesting connectionPath while not all paths are checked.");

            return protocol + "://" + mProxyHost + ":" + mProxyPort + "/proxy/" + directUrl;
        } else {
            StringBuilder urlBuilder = new StringBuilder();

            urlBuilder.append(protocol);
            urlBuilder.append("://");
            urlBuilder.append(mProxyHost);
            urlBuilder.append(":");
            urlBuilder.append(mProxyPort);

            if (targetServer != null && mProxyUuid != null && !mProxyUuid.equals(targetServer.getId())) {
                urlBuilder.append("/proxy/");
                urlBuilder.append(protocol);
                urlBuilder.append("/");
                urlBuilder.append(targetServer.getCompatibleId());
            }

            urlBuilder.append("/");

            return urlBuilder.toString();
        }
    }

    public String getServerApiUrl(ServerResource targetServer) {
        return getServerUrl(targetServer, "http");
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

}
