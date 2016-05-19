package com.networkoptix.hdwitness.api.data;

import java.io.Serializable;
import java.util.UUID;

import com.networkoptix.hdwitness.common.DeviceInfo;
import com.networkoptix.hdwitness.common.HttpHeaders;
import com.networkoptix.hdwitness.common.Utils;

import android.content.Context;
import android.util.Base64;

public class SessionInfo implements Serializable {
    private static final long serialVersionUID = 1148808655539394801L;
    
    
    public String Title;
    public String Login;
    public String Password;
    public String Hostname;
    public int Port;
    public long TimeDiff = 0;
    public final UUID RuntimeGuid = UUID.randomUUID();
    
    private HttpHeaders mCustomHeaders = new HttpHeaders();
    
    public SessionInfo() {
    }
    
    public SessionInfo(String title, String userName, String password, String hostName, int port) {
        Title = title;
        Login = userName;
        Password = password;
        Hostname = hostName;
        Port = port;
    }
    
    public void fillCustomHeaders(Context context) {
        String auth = Login + ":" + Password;
        mCustomHeaders.add("Authorization", "Basic " + Base64.encodeToString(auth.getBytes(), Base64.DEFAULT));
        mCustomHeaders.add("X-runtime-guid", RuntimeGuid.toString());
        mCustomHeaders.add("User-Agent", DeviceInfo.getUserAgent(context));
        mCustomHeaders.add("X-Nx-User-Name", Login);
    }
    
   
    @Override
    public boolean equals(Object o) {
        if (!(o instanceof SessionInfo))
            return false;
        SessionInfo ci = (SessionInfo)o;
        return ci.Login.equals(Login) &&
                ci.Password.equals(Password) &&
                ci.Hostname.equals(Hostname) &&
                ci.Port == Port ;
    }

    public HttpHeaders getCustomHeaders() {       
        return mCustomHeaders;
    }
    
    public String title() {
        String result = Title;
        if (result == null || result.length() == 0)
            result = Hostname;
        if (result == null)
            result = "";
        return result;
    }
    
    @Override
    public String toString() {
        return title();
    }
    
    public String toDetailedString() {
        StringBuilder sb = new StringBuilder();
        sb.append(Title);
        sb.append("(");
        sb.append(Hostname);
        sb.append(":");
        sb.append(Port);
        sb.append(") as ");
        sb.append(Login);
        sb.append(" [");
        sb.append(RuntimeGuid);
        sb.append("]");
        
        return sb.toString(); 
    }

    public String getDigest() {
        String source = Login + ":NetworkOptix:" + Password;
        String auth = Login + ":0:" + Utils.Md5(source);
        String digest = Base64.encodeToString(auth.getBytes(), Base64.NO_WRAP);
        return digest;
    }
    
}
