package com.networkoptix.hdwitness.ui;

import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;

import android.app.Application;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Handler;
import android.os.Message;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.api.data.ApiFactoryInterface.ServerInfoFactoryInterface;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.api.ec2.ApiTimeParser;
import com.networkoptix.hdwitness.api.ec2.TransactionLogParser;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.CameraHistory;
import com.networkoptix.hdwitness.api.resources.ResourcesList;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.UserResource;
import com.networkoptix.hdwitness.api.resources.actions.AbstractResourceAction;
import com.networkoptix.hdwitness.common.SoftwareVersion;
import com.networkoptix.hdwitness.common.network.ConnectTask;
import com.networkoptix.hdwitness.common.network.ConnectThread;
import com.networkoptix.hdwitness.common.network.Constants;
import com.networkoptix.hdwitness.common.network.StreamLoadingTask;
import com.networkoptix.hdwitness.core.ResourcesManager;
import com.networkoptix.hdwitness.core.ResourcesManager.OnServerResolvedListener;

public class HdwApplication extends Application {

    public static final int MESSAGE_CONNECTED = 0;
    public static final int MESSAGE_CONNECT_ERROR = 1;
    public static final int MESSAGE_RESOURCES_ACTION = 3;
    public static final int MESSAGE_SPLASH = 4;
    public static final int MESSAGE_TIME_DIFF = 5;

    public static final String BUNDLE_RESOURCES = "resources";

    private ResourcesManager mResources = new ResourcesManager();

    private SessionInfo mSessionInfo = null;
    private ServerInfo mServerInfo = null;

    private ConnectTask mConnectTask = null;
    private StreamLoadingTask mResourcesTask = null;
    
    private static class AppHttpResponseHandler extends Handler {
        private final WeakReference<HdwApplication> mApp;

        public AppHttpResponseHandler(HdwApplication app) {
            mApp = new WeakReference<HdwApplication>(app);
        }

        @Override
        public void handleMessage(Message msg) {
            HdwApplication app = mApp.get();
            if (app == null)
                return;
            app.handleHttpResponse(msg);
        }
    }

    private final AppHttpResponseHandler mHandler = new AppHttpResponseHandler(this);
    private ConnectThread mEventsListener = null;

    public ResourcesManager getHdwResources() {
        return mResources;
    }

    public SessionInfo getSessionInfo() {
        return mSessionInfo;
    }

    public ServerInfo getServerInfo() {
        return mServerInfo;
    }

    public void logoff() {
        Log("Logoff");
        if (mConnectTask != null)
            mConnectTask.cancel(true);
        mConnectTask = null;

        if (mEventsListener != null) {
            mEventsListener.clear();
            mEventsListener.interrupt();
        }
        mEventsListener = null;
        mResources.clear();
        mResources.setOnServerResolvedListener(null);
        LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(UiConsts.BCAST_LOGOFF));
    }

    private void handleHttpResponse(Message msg) {
        switch (msg.what) {
        case MESSAGE_RESOURCES_ACTION:
            resourcesAction((AbstractResourceAction) msg.obj);
            break;
        case MESSAGE_TIME_DIFF:
            mSessionInfo.TimeDiff = (Long)msg.obj;
            Log("Time diff updated to " + mSessionInfo.TimeDiff);
            break;
        }
    }

    private void resourcesAction(AbstractResourceAction action) {
        if (action == null)
            return; // test for actions such as Initial
        mResources.applyAction(action);
        LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(UiConsts.BCAST_RESOURCES_UPDATED));
    }

    private void updateResourcesList(ResourcesList resources) {
        mResources.clear();
        mResources.update(resources);
        LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(UiConsts.BCAST_RESOURCES_UPDATED));
    }

    protected void updateCameraHistory(CameraHistory history) {
        mResources.getHistory().clear();
        if (history != null)
            mResources.getHistory().merge(history);
    }

    public void requestConnect(final Handler handler, final SessionInfo data) {
        if (mConnectTask != null)
            mConnectTask.cancel(true);

        final ServerInfoFactoryInterface ec2Factory = new com.networkoptix.hdwitness.api.ec2.ConnectInfoFactory();

        Log("Trying to connect using ec2...");
        mConnectTask = (ConnectTask) new ConnectTask(data, getAppVersion(), ec2Factory) {
            @Override
            protected void onPostExecute(Integer result) {
                if (isCancelled()) {
                    Log("Connection cancelled");
                    return;
                }

                if (result == Constants.SUCCESS) {
                    Log("EC2 connection established.");
                    initiateEc2Connection(data, mServerInfo, handler);
                } else {
                    Log("Trying to connect using protobuf...");
                    final ServerInfoFactoryInterface protobufFactory = new com.networkoptix.hdwitness.api.protobuf.ConnectInfoFactory();

                    mConnectTask = (ConnectTask) new ConnectTask(data, getAppVersion(), protobufFactory) {
                        @Override
                        protected void onPostExecute(Integer result) {
                            if (isCancelled()) {
                                Log("Connection cancelled");
                                return;
                            }

                            /* Starting from this version protobuf protocol is not used anymore. */
                            SoftwareVersion ec2Version = new SoftwareVersion(2, 3);

                            if (result == Constants.SUCCESS 
                                    && mServerInfo != null 
                                    && mServerInfo.Version.lessThan(ec2Version)) {
                                Log("Protobuf connection established.");
                                initiateProtobufConnection(data, mServerInfo, handler);
                            } else {
                                int error = result.intValue();
                                if (result == Constants.SUCCESS) {
                                    Log("Connecting to " + mServerInfo.Version.toString() + " via protobuf failed.");
                                    error = Constants.IO_ERROR;
                                } else {
                                    Log("Connection failed.");
                                }
                                handler.obtainMessage(MESSAGE_CONNECT_ERROR, error, 0).sendToTarget();
                            }
                        }
                    }.execute("/api/connect/", "format=pb");
                }
            }
        }.execute("/ec2/connect/", "format=json");
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

    private void initiateEc2Connection(SessionInfo sessionInfo, ServerInfo serverInfo, final Handler handler) {
        Log("Initiating ec2 connection");
        if (mSessionInfo != sessionInfo)
            mResources.clear();

        mSessionInfo = sessionInfo;
        mServerInfo = serverInfo;
        mResources.setOnServerResolvedListener(null);
        mResources.setMediaProxy(sessionInfo.Hostname, sessionInfo.Port, serverInfo.Uuid);
        mResources.setResolvingEnabled(false);

        if (mResourcesTask != null)
            mResourcesTask.cancel(true);
        
        mResourcesTask = (StreamLoadingTask) new StreamLoadingTask(mSessionInfo) {
            
            @Override
            protected int parseStream(InputStream stream) {
                ApiTimeParser parser = new ApiTimeParser(stream);
                long localTime = System.currentTimeMillis();
                long serverTime = parser.getServerTime();
                Long diff = serverTime - localTime; //Object as we will send it between threads
                if (isCancelled())
                    return Constants.CANCELLED;

                mHandler.obtainMessage(MESSAGE_TIME_DIFF, diff).sendToTarget();
                return Constants.SUCCESS;
            }
            
        }.execute("/api/gettime/", "format=json");

        if (mEventsListener != null)
            mEventsListener.interrupt();
        mEventsListener = new ConnectThread(mSessionInfo, "http" , "/ec2/events/", "format=json&isMobile=true&runtime-guid=" + mSessionInfo.RuntimeGuid.toString());
        mEventsListener.setOnStreamReceivedListener(new ConnectThread.OnStreamReceivedListener() {
            private boolean mIsInterrupted = false;

            @Override
            public void onStreamReceived(InputStream stream) throws IOException {
                handler.obtainMessage(MESSAGE_CONNECTED).sendToTarget();
                
                TransactionLogParser parser = new TransactionLogParser(stream, mServerInfo.Version, mServerInfo.ProtocolVersion);
                while (!mIsInterrupted) {
                    AbstractResourceAction action = parser.nextAction();
                    if (action == null)
                        continue;
                    mHandler.obtainMessage(MESSAGE_RESOURCES_ACTION, action).sendToTarget();
                }

            }

            @Override
            public void onInterrupted() {
                if (mIsInterrupted)
                    return;

                Log("Connection interrupted");
                handler.obtainMessage(MESSAGE_CONNECT_ERROR, Constants.IO_ERROR, 0).sendToTarget();
                mIsInterrupted = true;
                logoff();
            }

            @Override
            public void onError(int error) {
                if (mIsInterrupted)
                    return;

                Log("Connection error " + error);
                handler.obtainMessage(MESSAGE_CONNECT_ERROR, error, 0).sendToTarget();
                mIsInterrupted = true;
                logoff();
            }
        });
        Log("Starting connect process");
        mEventsListener.start();

    }

    private void initiateProtobufConnection(SessionInfo sessionInfo, ServerInfo serverInfo, final Handler handler) {
        if (mSessionInfo != sessionInfo)
            mResources.clear();

        mSessionInfo = sessionInfo;
        mServerInfo = serverInfo;

        mResources.setOnServerResolvedListener(new OnServerResolvedListener() {
            @Override
            public void onServerConfirmed(ServerResource server) {
                LocalBroadcastManager.getInstance(HdwApplication.this).sendBroadcast(new Intent(UiConsts.BCAST_RESOURCES_UPDATED));
            }
        });
        mResources.setMediaProxy(mSessionInfo.Hostname, mServerInfo.ProxyPort, serverInfo.Uuid);
        mResources.setResolvingEnabled(true);

        if (mResourcesTask != null)
            mResourcesTask.cancel(true);

        mResourcesTask = (StreamLoadingTask) new StreamLoadingTask(mSessionInfo) {

            private ResourcesList mResources = null;

            @Override
            protected int parseStream(InputStream stream) {
                handler.obtainMessage(MESSAGE_CONNECTED).sendToTarget();
                
                com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser parser = new com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser(
                        stream);
                mResources = parser.resourcesList();
                if (isCancelled())
                    return Constants.CANCELLED;

                return Constants.SUCCESS;
            }

            @Override
            protected void onPostExecute(Integer result) {
                if (result == Constants.CANCELLED)
                    return;

                updateResourcesList(mResources);
                loadProtobufCameraHistory();
            }

        }.execute("/api/resource/", "format=pb");

        if (mEventsListener != null)
            mEventsListener.interrupt();
        mEventsListener = new ConnectThread(mSessionInfo, "https", "/events/", "format=pb");
        mEventsListener.setOnStreamReceivedListener(new ConnectThread.OnStreamReceivedListener() {
            private boolean mIsInterrupted = false;

            @Override
            public void onStreamReceived(InputStream stream) throws IOException {
                com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser parser = new com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser(
                        stream);
                while (!mIsInterrupted) {
                    AbstractResourceAction action = parser.nextAction();
                    if (action == null)
                        continue;
                    mHandler.obtainMessage(MESSAGE_RESOURCES_ACTION, action).sendToTarget();
                }
            }

            @Override
            public void onInterrupted() {
                if (mIsInterrupted)
                    return;

                mIsInterrupted = true;
                logoff();
            }

            @Override
            public void onError(int id) {
                if (mIsInterrupted)
                    return;

                mIsInterrupted = true;
                logoff();
            }
        });
        mEventsListener.start();

    }

    private void loadProtobufCameraHistory() {
        if (mResourcesTask != null)
            mResourcesTask.cancel(true);

        mResourcesTask = (StreamLoadingTask) new StreamLoadingTask(mSessionInfo) {

            private CameraHistory mHistory = null;

            @Override
            protected int parseStream(InputStream stream) {
                com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser parser = new com.networkoptix.hdwitness.api.protobuf.ResourcesStreamParser(
                        stream);
                mHistory = parser.cameraHistory();
                if (isCancelled())
                    return Constants.CANCELLED;

                return Constants.SUCCESS;
            }

            @Override
            protected void onPostExecute(Integer result) {
                if (result == Constants.CANCELLED)
                    return;

                updateCameraHistory(mHistory);
            }

        }.execute("/api/cameraServerItem/", "format=pb");
    }

    private String getAppVersion() {
        /** Current android app version */
        String appVersion = "2.3"; // TODO: #GDM read from Version.java
        PackageManager manager = getPackageManager();
        PackageInfo packInfo;
        try {
            packInfo = manager.getPackageInfo(getPackageName(), 0);
            appVersion = packInfo.versionName;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return appVersion;
    }

    public UserResource getCurrentUser() {
        for (AbstractResource user : getHdwResources().getUsers()) {
            if (user.getName().equalsIgnoreCase(mSessionInfo.Login))
                return (UserResource) user;
        }
        return null;
    }

}
