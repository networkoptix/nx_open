package com.networkoptix.hdwitness.api.resources;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import android.net.Uri;
import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;

public class ServerResource extends AbstractResource {
    private static final long serialVersionUID = 7581413417349650808L;

    /**
     * All server's network interfaces that are not checked.
     */
    private List<String> mUnchekedNetAddrList = new ArrayList<String>();

    /**
     * Default hostname. May be unavailable.
     */
    private String mHostname;

    /**
     * Resolved (available) hostname.
     */
    private String mResolvedHostname;

    /**
     * Network port.
     */
    private int mPort;

    public ServerResource(UUID id) {
        super(id, ResourceType.Server);
    }

    @Override
    public void setUrl(String url) {
        super.setUrl(url);

        Uri uri = Uri.parse(url);
        mHostname = uri.getHost();
        mPort = uri.getPort();
    }

    @Override
    public void update(AbstractResource resource) {
        super.update(resource);
        Uri uri = Uri.parse(getUrl());
        mHostname = uri.getHost();
        mPort = uri.getPort();
    }

    /**
     * Check if we have direct access to server.
     * 
     * @return True if we can reach server without using a proxy.
     */
    public boolean isResolved() {
        return mResolvedHostname != null;
    }

    /**
     * Check that we do not have direct way to server.
     * 
     * @return True if we cannot reach server by any of its interfaces.
     */
    public boolean isResolvedToProxy() {
        return mResolvedHostname == null && mUnchekedNetAddrList.isEmpty();
    }

    public int getPort() {
        return mPort;
    }

    public void setUncheckedNetAddrs(List<String> list) {
        mUnchekedNetAddrList.clear();
        mUnchekedNetAddrList.addAll(list);
    }

    public List<String> uncheckedNetAddrs() {
        return mUnchekedNetAddrList;
    }

    public void confirmHostname(String hostname) {
        if (isResolved()) {
            Log("Another server interface confirmed: " + hostname + " (already confirmed " + mResolvedHostname + ")");
            return;
        }
        mResolvedHostname = hostname;
    }

    public void disbandHostname(String hostname) {
        mUnchekedNetAddrList.remove(hostname);
    }

    public String getHostname() {
        if (mResolvedHostname != null)
            return mResolvedHostname;
        return mHostname;
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }
    
    @Override
    public String toString() {
        return this.getClass().getSimpleName() + ":" + getId() + ":" + getHostname() + ":" + getPort();
    }

}
