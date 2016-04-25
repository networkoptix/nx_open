package com.networkoptix.hdwitness.common.network;

//TODO: #GDM rename
public final class Constants {

    public static final int SUCCESS = 0;
    public static final int CANCELLED = -1;
    public static final int URI_ERROR = -2;
    public static final int PROTOCOL_ERROR = -3;
    public static final int IO_ERROR = -4;
    public static final int CREDENTIALS_ERROR = -5;
    public static final int BRAND_ERROR = -6;    
    
    public static final int ERROR_BASE = -100;

    /** Http status codes */
    public static final int STATUS_OK = 200;
    public static final int STATUS_CREDENTIALS = 401;
    public static final int STATUS_UNAUTHORIZED = 403;
    public static final int STATUS_NOT_FOUND = 404; 

    /** Default http connection timeout. */
    protected static final int DEFAULT_TIMEOUT = 30000;

    public static Integer statusCodeToError(int statusCode) {
        switch(statusCode) {
        case STATUS_CREDENTIALS:
        case STATUS_UNAUTHORIZED:
            return CREDENTIALS_ERROR;
        case STATUS_NOT_FOUND:
            return URI_ERROR;
        default:
            return statusCode;
        }
    } 
    
}
