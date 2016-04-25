package com.networkoptix.hdwitness.api.data;

import java.util.UUID;

import com.networkoptix.hdwitness.common.SoftwareVersion;

public class ServerInfo {
 
    public static final int PROTOCOL_2_3 = 1000;
    public static final int PROTOCOL_2_4 = 1026;
    
    /**
     * MediaProxy port
     */
    public int ProxyPort = 0;

    /**
     * Flag set to true if this EC is compatible with the current android client.
     */
    public boolean Compatible = false;
    
    /**
     * Name of the server's customization. 
     */
    public String Brand;
    
    /**
     * Version of the protocol. Servers 2.2 and older have protocol version less than 1000;
     */
    public int ProtocolVersion = 0;
    
    /** 
     * UUID of the server. 
     */
    public UUID Uuid;
    
    /**
     * Version of the server.
     */
    public SoftwareVersion Version;
    
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(Brand);
        sb.append(" Server ");
        sb.append(Version.toString());
        sb.append(" proto ");
        sb.append(ProtocolVersion);
        sb.append(" uuid ");
        sb.append(Uuid);
        if (!Compatible)
        sb.append(" not compatible");
        return sb.toString();
    }
}
