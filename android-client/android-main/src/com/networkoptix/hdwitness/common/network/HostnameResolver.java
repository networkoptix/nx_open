package com.networkoptix.hdwitness.common.network;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.Executor;

import android.util.Log;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.common.Utils;

public class HostnameResolver {

    private static final Executor mConnectTaskExecutor = StreamLoadingTask.createExecutor(5);

    public enum ResolveStatus {
        InProgress, Resolved, Disbanded;

        @Override
        public String toString() {
            switch (this) {
            case Disbanded:
                return "Disbanded";
            case InProgress:
                return "InProgress";
            case Resolved:
                return "Resolved";
            default:
                return "Unknown";
            }
        }
    }
    
    private List<StreamLoadingTask> mActiveTasks = new ArrayList<StreamLoadingTask>(); 

    public static class HostnameInfo {

        public final UUID Guid;
        public final int Port;
        public final String Hostname;

        public HostnameInfo(UUID guid, int port, String hostname) {
            Guid = guid;
            Port = port;
            Hostname = hostname;
        }

        public String address() {
            return Hostname + ":" + Port;
        }

        @Override
        public String toString() {
            return Guid + " (" + Hostname + ":" + Port + ")";
        }

    }

    public interface OnResolveListener {
        void resolved(HostnameInfo info);

        void disbanded(HostnameInfo info);
    }

    private HashMap<String, ResolveStatus> mHostnames = new HashMap<String, ResolveStatus>();
    private OnResolveListener mOnResolveListener = null;

    public void setOnResolveListener(OnResolveListener listener) {
        mOnResolveListener = listener;
    }

    public void handleServerError(HostnameInfo info) {
        // if logged off while checking
        if (!mHostnames.containsKey(info.toString()))
            return;

        mHostnames.put(info.toString(), ResolveStatus.Disbanded);
        if (mOnResolveListener != null)
            mOnResolveListener.disbanded(info);
    }

    public void handleServerResolved(HostnameInfo info) {
        // if logged off while checking
        if (!mHostnames.containsKey(info.toString()))
            return;

        mHostnames.put(info.toString(), ResolveStatus.Resolved);
        if (mOnResolveListener != null)
            mOnResolveListener.resolved(info);
    }

    public ResolveStatus resolveHostname(final String hostname, final int port, final UUID guid) {

        HostnameInfo info = new HostnameInfo(guid, port, hostname);

        Log("Resolve request " + info.toString());
        if (mHostnames.containsKey(info.toString())) {
            ResolveStatus status = mHostnames.get(info.toString());
            Log("cached result " + status.toString() + " : " + info.toString());
            return status;
        }

        StringBuilder urlBuilder = new StringBuilder().append("http://").append(info.address()).append("/api/ping/");
        Log("requesting " + urlBuilder.toString());

        StreamLoadingTask task = new StreamLoadingTask(5000) {
            @Override
            protected int parseStream(InputStream stream) {
                String pong = Utils.convertStreamToString(stream);

                if (pong.contains(guid.toString())          /* 2.3 and newer */
                        || pong.contains("<pong>"))         /* 2.2 and older */
                    return Constants.SUCCESS;
                return Constants.CREDENTIALS_ERROR;
            }

            protected void onPostExecute(Integer result) {
                mActiveTasks.remove(this);
                HostnameInfo hostnameInfo = new HostnameInfo(guid, port, hostname);

                Log("Server resolve result " + result.toString() + ":" + hostnameInfo.toString());

                if (result == Constants.SUCCESS || result == Constants.STATUS_NOT_FOUND)
                    handleServerResolved(hostnameInfo);
                else
                    handleServerError(hostnameInfo);
            };
            
            @Override
            protected void onCancelled(Integer result) {
                super.onCancelled(result);
                mActiveTasks.remove(this);
            }
        };
        Log("Adding ping executor task");
        mActiveTasks.add(task);
        task.executeOnExecutor(mConnectTaskExecutor, urlBuilder.toString(), "");

        mHostnames.put(info.toString(), ResolveStatus.InProgress);
        return ResolveStatus.InProgress;
    }

    public void clear() {
        Log("Clearing ping executor tasks");
        mHostnames.clear();
        for (StreamLoadingTask task: mActiveTasks) {
            task.cancel(true);
            Log("Task cancelled");
        }
        mActiveTasks.clear();
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }

}
