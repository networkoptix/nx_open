package com.networkoptix.hdwitness.common.network;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;

import org.apache.http.HttpResponse;
import org.apache.http.auth.AuthScope;
import org.apache.http.auth.UsernamePasswordCredentials;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.CredentialsProvider;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.AbstractHttpClient;
import org.apache.http.impl.client.BasicCredentialsProvider;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.HttpConnectionParams;
import org.apache.http.params.HttpParams;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.common.AsyncTask;
import com.networkoptix.hdwitness.common.HttpHeaders;

public abstract class StreamLoadingTask extends
		AsyncTask<String, Void, Integer> {
  
	private SessionInfo mConnectInfo = null;
	private int mTimeout = Constants.DEFAULT_TIMEOUT;
	
	public StreamLoadingTask(SessionInfo connectInfo) {
        mConnectInfo = connectInfo;
        if (mConnectInfo.getCustomHeaders().isEmpty())
            throw new AssertionError();        
    }

	public StreamLoadingTask(SessionInfo connectInfo, int timeout) {
		mConnectInfo = connectInfo;
		mTimeout = timeout;
		if (mConnectInfo.getCustomHeaders().isEmpty())
		    throw new AssertionError();
	}
	
	   public StreamLoadingTask(int timeout) {
	        mTimeout = timeout;
	    }

	@Override
	protected Integer doInBackground(String... params) {
	    URI uri;
  
		String path = params[0];

		if (BuildConfig.DEBUG)
		    Log.d("StreamLoadingTask", "stream loading task started for: " + path);
		
		try {
		    if (path.startsWith("/") && mConnectInfo != null) {    //relative path
		        String query = params[1];
	            uri = new URI("https", null, mConnectInfo.Hostname, mConnectInfo.Port, path, query, null);
		    }
	        else {
	            uri = new URI(path);
	        }
		} catch (URISyntaxException e) {
			e.printStackTrace();
			return Constants.URI_ERROR;
		}

		final HttpParams httpParams = new BasicHttpParams();
	    HttpConnectionParams.setConnectionTimeout(httpParams, mTimeout);
		
		HttpClient client;
		if (uri.getScheme().equals("https")) {
			CredentialsProvider credProvider = new BasicCredentialsProvider();
			credProvider.setCredentials(new AuthScope(AuthScope.ANY_HOST,
					AuthScope.ANY_PORT), new UsernamePasswordCredentials(
					mConnectInfo.Login, mConnectInfo.Password));

			client = UnsecureSSLSocketFactory.getUnsecureHttpClient(httpParams);
			((AbstractHttpClient) client).setCredentialsProvider(credProvider);
		} else
			client = new DefaultHttpClient(httpParams);

		HttpGet request = new HttpGet(uri);
		request.setParams(httpParams);

		if (mConnectInfo != null) {
            for (HttpHeaders.HttpHeader header: mConnectInfo.getCustomHeaders())              
                request.addHeader(header.key, header.value);
		}

		HttpResponse response;
		try {
			if (isCancelled())
				return Constants.CANCELLED;
			//long operation
			response = client.execute(request);
			if (isCancelled())
				return Constants.CANCELLED;

			switch (response.getStatusLine().getStatusCode()) {
			case Constants.STATUS_OK:
				return parseStream(response.getEntity().getContent());
			default:
			    return Constants.statusCodeToError(response.getStatusLine().getStatusCode());

			}
		} catch (ClientProtocolException e) {
			return Constants.PROTOCOL_ERROR;
		} catch (IOException e) {
			return Constants.IO_ERROR;
		}
	}

	protected abstract int parseStream(InputStream stream);

}
