package com.networkoptix.hdwitness.ui.fragments;

import java.util.ArrayList;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.ExpandableListView.OnChildClickListener;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.DisplayObject;
import com.networkoptix.hdwitness.ui.DisplayObjectList;
import com.networkoptix.hdwitness.ui.adapters.ResourcesListAdapter;

public class ResourcesTreeFragment extends AbstractResourcesFragment {
    
    private ResourcesListAdapter mAdapter;
    private ExpandableListView mListView;
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        mAdapter  = new ResourcesListAdapter(getActivity());
    }
    
    @Override
    public void onResume() {
        super.onResume();
    }
    

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View v = inflater.inflate(R.layout.fragment_resources_tree, container, false);
   
        mListView = (ExpandableListView) v.findViewById(R.id.listResources);
        mListView.setAdapter((ExpandableListAdapter)mAdapter);
        mListView.setOnChildClickListener(new OnChildClickListener() {
            
            @Override
            public boolean onChildClick(ExpandableListView parent, View v, int groupPosition, int childPosition, long id) {
                DisplayObject cameraObj = (DisplayObject) mAdapter.getChild(groupPosition, childPosition);
                if (cameraObj.Id == null)
                    return false;

                openCamera(v, cameraObj.Id);
                return true;
            }
        });
        return v;
    }

    @Override
    public void updateData(ArrayList<DisplayObjectList> data) {
        boolean wasEmpty = mAdapter.isEmpty();
        mAdapter.updateData(data);
        mAdapter.notifyDataSetChanged();
        if (wasEmpty && mListView != null)
            for (int i = 0; i < mAdapter.getGroupCount(); i++)
                mListView.expandGroup(i);
    }

   
}
