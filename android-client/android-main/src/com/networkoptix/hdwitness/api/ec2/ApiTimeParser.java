package com.networkoptix.hdwitness.api.ec2;

import java.io.InputStream;

import android.util.Log;

import com.google.gson.Gson;
import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.common.Utils;

public class ApiTimeParser {

    /*
    {
        "error": "0",
        "errorString": "",
        "reply": {
            "realm": "networkoptix",
            "timeZoneOffset": "10800000",
            "timezoneId": "Asia/Jerusalem",
            "utcTime": "1441709266174"
        }
    }
    */
    
    final private class Reply {
        long utcTime;
    }

    final private class JsonWrapper {
        int error;
        String errorString;
        Reply reply;
    }

    private long mServerTime = 0;
    
    public ApiTimeParser(InputStream stream) {
        String json = Utils.convertStreamToString(stream);
        Log(json);
        
        final Gson gson = new Gson();
        
        JsonWrapper wrapper = gson.fromJson(json, JsonWrapper.class);
        if (wrapper.error != 0) {
            Log.e("ApiTimeParser", wrapper.error + ":" + wrapper.errorString);
            return;
        }
        
        mServerTime  = wrapper.reply.utcTime;
        Log(String.valueOf(mServerTime));
    }

    public long getServerTime() {
        return mServerTime;
    }

    private void Log(String message) {
        if (BuildConfig.DEBUG)
            Log.d("ApiTimeParser", message);
    }
}
