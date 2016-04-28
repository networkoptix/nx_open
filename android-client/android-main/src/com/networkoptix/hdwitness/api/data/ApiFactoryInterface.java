package com.networkoptix.hdwitness.api.data;

import java.io.IOException;
import java.io.InputStream;

public interface ApiFactoryInterface {
    
    
    public static interface ServerInfoFactoryInterface {
        public abstract ServerInfo createConnectInfo(InputStream stream, String appVersion) throws IOException;
    }
    
}
