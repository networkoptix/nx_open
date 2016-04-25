package com.networkoptix.hdwitness.api.ec2;

import java.io.IOException;
import java.io.InputStream;

import com.networkoptix.hdwitness.api.data.ApiFactoryInterface.ServerInfoFactoryInterface;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.common.CompatibilityChecker;
import com.networkoptix.hdwitness.common.SoftwareVersion;
import com.networkoptix.hdwitness.common.Utils;

import com.google.gson.Gson;

public class ConnectInfoFactory implements ServerInfoFactoryInterface {

    private class ServerInfoWrapper {
        String brand;
        String version;
        String ecsGuid;
        int nxClusterProtoVersion;
    }
    
    @Override
    public ServerInfo createConnectInfo(InputStream stream, String appVersion) throws IOException {
        ServerInfo result = new ServerInfo();
        
        ServerInfoWrapper info;
        Gson gson = new Gson();
        try {
            info = gson.fromJson(Utils.convertStreamToString(stream), ServerInfoWrapper.class);
        } catch (com.google.gson.JsonSyntaxException e) {
            e.printStackTrace();
            return null;
        }
        
        result.Brand = info.brand;
        result.Compatible = CompatibilityChecker.localCompatibilityCheck(info.version); //TODO: #GDM use compatibilityItems when implemented on server side
        result.ProtocolVersion = info.nxClusterProtoVersion;
        result.Uuid = Utils.uuid(info.ecsGuid);
        result.Version = new SoftwareVersion(info.version);
        
        return result;
        
    }

}
