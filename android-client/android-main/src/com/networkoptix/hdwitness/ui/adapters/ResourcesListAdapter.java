package com.networkoptix.hdwitness.ui.adapters;

import java.util.ArrayList;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ExpandableListAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.ui.DisplayObject;
import com.networkoptix.hdwitness.ui.DisplayObjectList;

public class ResourcesListAdapter extends BaseAdapter implements ExpandableListAdapter{

	ArrayList<DisplayObjectList> mData = new ArrayList<DisplayObjectList>();
	ArrayList<DisplayObject> mDataSimplified = new ArrayList<DisplayObject>();
	
	private final LayoutInflater mInflater;

	public ResourcesListAdapter(Context context) {
	    mInflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
	}
	
	public void updateData(ArrayList<DisplayObjectList> data){
		mData.clear();
		mData.addAll(data);
		
		mDataSimplified.clear();
		for (DisplayObjectList list: data) {
		    mDataSimplified.add(list);
		    mDataSimplified.addAll(list.Subtree);
		}
		
		notifyDataSetChanged();
	}

	public boolean areAllItemsEnabled() {
		return true;
	}

	/**
	 * @returns corresponding CameraResource
	 */
	public Object getChild(int groupPosition, int childPosition) {
		return mData.get(groupPosition).Subtree.get(childPosition);
	}

	public long getChildId(int groupPosition, int childPosition) {
	    return (groupPosition << 16) + childPosition;
	}

	public View getChildView(int groupPosition, int childPosition,
			boolean isLastChild, View convertView, ViewGroup parent) {
        if (mInflater == null)
            return null;
        	    
        if (convertView == null) {
            convertView = mInflater.inflate(R.layout.resource_list_item, null);
        } 
        TextView t = (TextView) convertView.findViewById(R.id.text_resource);
        ImageView im1 = (ImageView) convertView.findViewById(R.id.icon_resource);
       	
        DisplayObject display = (DisplayObject) getChild(groupPosition, childPosition);
        switch (display.Status){
        case Offline:
        	im1.setImageResource(R.drawable.resource_camera_offline);
        	break;
		case Online:
			im1.setImageResource(R.drawable.resource_camera);
			break;
		case Recording:
			im1.setImageResource(R.drawable.resource_camera_recording);
			break;
		case Unauthorized:
			im1.setImageResource(R.drawable.resource_camera_unauthorized);
			break;
		default:
			break;
        }
        t.setText(display.Caption);
        
        return convertView;
	}

	public int getChildrenCount(int groupPosition) {
		return mData.get(groupPosition).Subtree.size();
	}

	public long getCombinedChildId(long groupId, long childId) {
		return getCombinedGroupId(groupId) + childId;
	}

	public long getCombinedGroupId(long groupId) {
		return groupId << 16;
	}

	/**
	 * @returns Integer key of the corresponding server 
	 */
	public Object getGroup(int groupPosition) {
		return mData.get(groupPosition);
	}

	public int getGroupCount() {
		return mData.size();
	}

	public long getGroupId(int groupPosition) {
		return groupPosition;
	}

	public View getGroupView(int groupPosition, boolean isExpanded,
			View convertView, ViewGroup parent) {
	    
        if (mInflater == null)
            return null;
	    
        if (convertView == null) {
            convertView = mInflater.inflate(R.layout.resource_list_item, null);
        }
       
        TextView t = (TextView) convertView.findViewById(R.id.text_resource);
        ImageView im1 = (ImageView) convertView.findViewById(R.id.icon_resource);
       
		DisplayObjectList display = mData.get(groupPosition);

        switch (display.Status){
        case Offline:
            im1.setImageResource(R.drawable.resource_server_offline);
            break;
        case Online:
            im1.setImageResource(R.drawable.resource_server);
            break;
        case Recording:  // ugly hack to mark layout
            im1.setImageResource(R.drawable.resource_layout);
            break;
        default:
            break;
        }
        
		t.setText(display.Caption);

		return convertView;
	}

	public boolean hasStableIds() {
		return true;
	}

	public boolean isChildSelectable(int groupPosition, int childPosition) {
		return true;
	}

	public boolean isEmpty() {
		return mData.isEmpty();
	}

	public void onGroupCollapsed(int groupPosition) {
		// TODO Auto-generated method stub
		
	}

	public void onGroupExpanded(int groupPosition) {
		// TODO Auto-generated method stub
		
	}

	@Override
    public int getCount() {
        return mDataSimplified.size();
    }

    @Override
    public Object getItem(int position) {
        return mDataSimplified.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        return null;
    }
	
}