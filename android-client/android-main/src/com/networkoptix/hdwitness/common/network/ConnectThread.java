package com.networkoptix.hdwitness.common.network;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;

import org.apache.http.HttpEntity;
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

import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.common.HttpHeaders;

public class ConnectThread extends Thread {

	public static interface OnStreamReceivedListener {
		void onStreamReceived(InputStream stream) throws IOException;
		void onError(int id);
        void onInterrupted();
	}
	
	private final SessionInfo mConnectInfo;
	private final String mPath;
	private final String mProtocol;
	private final String mQuery;
	private final String mUrl;
	private OnStreamReceivedListener mListener = null;
	private HttpEntity mResponse = null;

	public ConnectThread(SessionInfo data, String protocol, String path, String query) {
		mConnectInfo = data;
		mPath = path;
		mProtocol = protocol;
		mQuery = query;
		mUrl = null;
        if (mConnectInfo.getCustomHeaders().isEmpty())
            throw new AssertionError();
	}

	public void setOnStreamReceivedListener(OnStreamReceivedListener listener) {
		mListener = listener;
	}

	@Override
	public void run() {
		URI uri;
		try {
			if (mUrl != null)
				uri = new URI(mUrl);
			else
				uri = new URI(mProtocol,
				        null,
				        mConnectInfo.Hostname,
						mConnectInfo.Port, mPath, mQuery, null);
		} catch (URISyntaxException e) {
			if (mListener != null)
				mListener.onError(Constants.URI_ERROR);
			else
				e.printStackTrace();
			return;
		}

        final HttpParams httpParams = new BasicHttpParams();
	    HttpConnectionParams.setConnectionTimeout(httpParams, Constants.DEFAULT_TIMEOUT);
		
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
			response = client.execute(request);
			if (mListener != null) {
				switch (response.getStatusLine().getStatusCode()) {
				case Constants.STATUS_OK:
					mResponse = response.getEntity();
					mListener.onStreamReceived(mResponse.getContent());
					break;
				default:
				    mListener.onError(Constants.statusCodeToError(response.getStatusLine().getStatusCode()));
					break;
				}
			}
		} catch (ClientProtocolException e) {
		    e.printStackTrace();
			if (mListener != null)
				mListener.onError(Constants.PROTOCOL_ERROR);
		} catch (IOException e) {
		    Log.e(this.getClass().getSimpleName(), "ERROR while executing request " + uri);
		    e.printStackTrace();
			if (mListener != null)
				mListener.onError(Constants.IO_ERROR);

		}
	}

	@Override
	public void interrupt() {
	    super.interrupt();
        if (mListener != null)
            mListener.onInterrupted();
	}

	/** Suppress following messages. */
    public void clear() {
        mListener = null;
    }
}
