package com.networkoptix.hdwitness.common.network;

import java.io.IOException;
import java.io.InputStream;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.Version;
import com.networkoptix.hdwitness.api.data.ApiFactoryInterface.ServerInfoFactoryInterface;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.api.data.SessionInfo;

public class ConnectTask extends StreamLoadingTask {

    protected ServerInfo mServerInfo = null;

    private final ServerInfoFactoryInterface mFactory;
    private final String mAppVersion;

    public ConnectTask(SessionInfo connectInfo, String appVersion, ServerInfoFactoryInterface factory) {
        super(connectInfo);
        mAppVersion = appVersion;
        mFactory = factory;
    }

    private boolean checkBrand(String brand) {
        if (BuildConfig.DEBUG)
            return true;
        
        if (brand == null || brand.length() == 0)
            return true;
        
        String local = Version.QN_PRODUCT_NAME_SHORT;
        if (brand.equalsIgnoreCase(local))
            return true;
        
        String[] parts = brand.split("[_-]");
        String[] localParts = local.split("[_-]");
        
        if (parts.length < 1 || localParts.length < 1)
            return false;
        
        return parts[0].equalsIgnoreCase(localParts[0]);
    }
    
    @Override
    protected int parseStream(InputStream stream) {

        try {
            mServerInfo = mFactory.createConnectInfo(stream, mAppVersion);
        } catch (IOException e) {
            e.printStackTrace();
            return Constants.IO_ERROR;
        }

        if (mServerInfo == null)
            return Constants.IO_ERROR;
        
        if (isCancelled())
            return Constants.CANCELLED;

        if (!checkBrand(mServerInfo.Brand))
            return Constants.BRAND_ERROR;  
        
        return Constants.SUCCESS;
    }

}
