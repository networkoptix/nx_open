package com.networkoptix.hdwitness.api.resources;

public enum Status {
    
    Invalid,
    Offline,
    Online,
    Recording,
    Unauthorized;
    
    public static Status fromString(String value) {
        if (value.equalsIgnoreCase("online"))
            return Status.Online;   
        if (value.equalsIgnoreCase("offline"))
            return Status.Offline;
        else if (value.equalsIgnoreCase("unauthorized"))
            return Status.Unauthorized;
        else if (value.equalsIgnoreCase("recording"))
            return Status.Recording;
        else 
            return Status.Invalid;
    }
}
