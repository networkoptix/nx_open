package com.networkoptix.hdwitness.ui.fragments;

import java.util.ArrayList;
import java.util.Collections;
import java.util.UUID;

import android.annotation.TargetApi;
import android.app.ActivityOptions;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.content.LocalBroadcastManager;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.resources.AbstractResource;
import com.networkoptix.hdwitness.api.resources.CameraResource;
import com.networkoptix.hdwitness.api.resources.LayoutResource;
import com.networkoptix.hdwitness.api.resources.ServerResource;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.api.resources.UserResource;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.core.ResourcesManager;
import com.networkoptix.hdwitness.ui.DisplayObject;
import com.networkoptix.hdwitness.ui.DisplayObjectList;
import com.networkoptix.hdwitness.ui.HdwApplication;
import com.networkoptix.hdwitness.ui.UiConsts;
import com.networkoptix.hdwitness.ui.activities.ResourcesActivity;
import com.networkoptix.hdwitness.ui.activities.VideoActivity;

public abstract class AbstractResourcesFragment extends Fragment {

    public abstract void updateData(ArrayList<DisplayObjectList> data);

    private BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!AbstractResourcesFragment.this.isResumed())
                return;
            
            if (intent.getAction().equals(UiConsts.BCAST_RESOURCES_UPDATED))
                update();
            else if (intent.getAction().equals(UiConsts.BCAST_SHOW_OFFLINE_UPDATED)) {
                updateShowOffline();
            }
        }
    };

    private boolean mShowOffline;

    private ResourcesActivity parentActivity() {
        return (ResourcesActivity) getActivity();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @TargetApi(11)
    @Override
    public void onResume() {
        super.onResume();
        LocalBroadcastManager.getInstance(getActivity()).registerReceiver(mBroadcastReceiver,
                new IntentFilter(UiConsts.BCAST_RESOURCES_UPDATED));
        LocalBroadcastManager.getInstance(getActivity()).registerReceiver(mBroadcastReceiver,
                new IntentFilter(UiConsts.BCAST_SHOW_OFFLINE_UPDATED));

        mShowOffline = parentActivity().getShowOfflineResources();

        if (Utils.hasHoneycomb())
            getActivity().invalidateOptionsMenu();

        if (getApp().getCurrentUser() != null)
            update();
    }

    @Override
    public void onPause() {
        LocalBroadcastManager.getInstance(getActivity()).unregisterReceiver(mBroadcastReceiver);
        super.onPause();
    }

    @TargetApi(16)
    protected void openCamera(View v, UUID id) {
        final Intent i = new Intent(getActivity(), VideoActivity.class);
        i.putExtra(VideoActivity.CAMERA_ID_UUID, id);
        if (Utils.hasJellyBean()) {
            // makeThumbnailScaleUpAnimation() looks kind of ugly here as the
            // loading spinner may
            // show plus the thumbnail image in GridView is cropped. so using
            // makeScaleUpAnimation() instead.
            ActivityOptions options = ActivityOptions.makeScaleUpAnimation(v, 0, 0, v.getWidth(), v.getHeight());
            getActivity().startActivity(i, options.toBundle());
        } else {
            startActivity(i);
        }
    }

    protected void update() {
        ArrayList<DisplayObjectList> displayTree = getUserDisplayTree();
        updateData(displayTree);
    }

    private ArrayList<DisplayObjectList> getAdminDisplayTree() {
        ArrayList<DisplayObjectList> result = new ArrayList<DisplayObjectList>();

        for (AbstractResource resource : getApp().getHdwResources().getAllServers()) {
            ServerResource server = (ServerResource) resource;

            if (!mShowOffline && (server.getStatus().equals(Status.Offline)))
                continue;

            DisplayObjectList serverDisplay = new DisplayObjectList();
            serverDisplay.Caption = server.getName() + " (" + server.getHostname() + ")";
            serverDisplay.Id = server.getId();
            serverDisplay.Status = server.getStatus();
            result.add(serverDisplay);

            for (CameraResource camera : getApp().getHdwResources().getCamerasByServerId(server.getId())) {
                if (camera.isDisabled())
                    continue;

                if (!mShowOffline && (camera.getStatus().equals(Status.Offline)))
                    continue;

                DisplayObject cameraDisplay = new DisplayObject();
                StringBuilder caption = new StringBuilder(camera.getName());
                if (camera.getUrl() != null && camera.getUrl().length() > 0)
                    caption.append(" (").append(camera.getUrl()).append(")");
                cameraDisplay.Caption = caption.toString();
                cameraDisplay.Status = camera.getStatus();
                cameraDisplay.Id = camera.getId();
                if (server.getStatus() == Status.Online)
                    cameraDisplay.Thumbnail = getApp().getHdwResources().getServerApiUrl(server) + "api/image" + "?physicalId="
                            + Uri.encode(camera.getPhysicalId()) + "&time=LATEST" + "&format=jpg";
                serverDisplay.addItem(cameraDisplay);
            }

            Collections.sort(serverDisplay.Subtree);
        }
        Collections.sort(result);

        return result;
    }

    protected HdwApplication getApp() {
        return (HdwApplication) getActivity().getApplication();
    }

    private ArrayList<DisplayObjectList> getUserDisplayTree() {
        ResourcesManager manager = getApp().getHdwResources();

        ArrayList<DisplayObjectList> result = new ArrayList<DisplayObjectList>();

        UserResource user = getApp().getCurrentUser();
        if (user == null) {
            // current user is still not loaded
            return result;
        }

        if (user.hasPermissions(UserResource.GlobalAdministratorPermission))
            return getAdminDisplayTree();

        for (LayoutResource layout : manager.getLayoutsByUserId(user.getId())) {
            DisplayObjectList layoutDisplay = new DisplayObjectList();
            layoutDisplay.Caption = layout.getName();
            layoutDisplay.Id = layout.getId();
            layoutDisplay.Status = Status.Recording; // ugly hack to mark
                                                     // layout
            result.add(layoutDisplay);

            for (UUID cameraId : layout.getCameraIdsList()) {
                CameraResource camera = manager.getEnabledCameraById(cameraId);
                if (camera == null)
                    continue;
                
                if (camera.isDisabled())
                    continue;
                
                if (!camera.hasVideo())
                    continue;

                if (!mShowOffline && (camera.getStatus().equals(Status.Offline)))
                    continue;

                ServerResource server = getApp().getHdwResources().getServerByCameraId(camera.getId());

                DisplayObject cameraDisplay = new DisplayObject();
                StringBuilder caption = new StringBuilder(camera.getName());
                if (camera.getUrl().length() > 0)
                    caption.append(" (").append(camera.getUrl()).append(")");
                cameraDisplay.Caption = caption.toString();
                cameraDisplay.Status = camera.getStatus();
                cameraDisplay.Id = camera.getId();
                cameraDisplay.Thumbnail = getApp().getHdwResources().getServerApiUrl(server) + "api/image" + "?physicalId="
                        + Uri.encode(camera.getPhysicalId()) + "&time=LATEST&format=jpg";
                layoutDisplay.addItem(cameraDisplay);
            }
            Collections.sort(layoutDisplay.Subtree);
        }
        Collections.sort(result);
        return result;
    }

    @TargetApi(11)
    private void updateShowOffline() {
        boolean showOffline = parentActivity().getShowOfflineResources();
        if (showOffline == mShowOffline)
            return;
        mShowOffline = showOffline;
        if (Utils.hasHoneycomb())
            getActivity().invalidateOptionsMenu();
        update();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.menu_resources_common, menu);
        super.onCreateOptionsMenu(menu, inflater);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case R.id.menu_show_offline:
            parentActivity().setShowOfflineResources(!mShowOffline);
            break;
        case R.id.menu_logout:
            getApp().logoff();
            getActivity().finish();
            break;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        MenuItem item = menu.findItem(R.id.menu_show_offline);
        if (item != null) {
            item.setCheckable(true);
            item.setChecked(mShowOffline);
            item.setTitle(mShowOffline ? R.string.menu_hide_offline : R.string.menu_show_offline);
            item.setIcon(mShowOffline ? R.drawable.ic_menu_hide_offline : R.drawable.ic_menu_view);
        }

        super.onPrepareOptionsMenu(menu);
    }

}
