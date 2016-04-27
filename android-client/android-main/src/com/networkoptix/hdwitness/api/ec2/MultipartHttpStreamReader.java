package com.networkoptix.hdwitness.api.ec2;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.ec2.TransactionLogParser.HttpStreamReader;

/**
 * @author gdm 
 * Implementation of the HTTP Multipart protocol.
 * @see http://www.w3.org/Protocols/rfc1341/7_2_Multipart.html
 */
public class MultipartHttpStreamReader implements HttpStreamReader {

    private static final String JSON_PREFIX = "{";
    
    private static final Charset UTF_8 = Charset.forName("UTF-8");

    private final BufferedReader mStreamReader;

    public MultipartHttpStreamReader(InputStream stream) {
        mStreamReader = new BufferedReader(new InputStreamReader(stream, UTF_8));
    }

    @Override
    public String readTransaction() throws IOException {
        String line = null;
        do {
            line = mStreamReader.readLine();
            if (line == null)
                return line;
           
            Log(line);
            if (line.startsWith(JSON_PREFIX))
                return line;
        } while (line != null);
        return line;
    }

    protected void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

}
