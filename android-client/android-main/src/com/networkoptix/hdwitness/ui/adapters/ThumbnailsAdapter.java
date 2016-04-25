package com.networkoptix.hdwitness.ui.adapters;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.UUID;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.AbsListView;
import android.widget.BaseAdapter;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.resources.Status;
import com.networkoptix.hdwitness.common.image.ImageFetcher;
import com.networkoptix.hdwitness.ui.DisplayObject;
import com.networkoptix.hdwitness.ui.DisplayObjectList;

/**
 * The main adapter that backs the GridView. This is fairly standard except the number of
 * columns in the GridView is used to create a fake top row of empty views as we use a
 * transparent ActionBar and don't want the real top row of images to start off covered by it.
 */
public class ThumbnailsAdapter extends BaseAdapter {

    public enum ItemType {
        Header,
        Header_Dummy,
        Camera,
        Camera_Dummy
    }
    
    private class GridDisplayObject extends DisplayObject {
        
        public GridDisplayObject(DisplayObjectList list) {
            Type = ItemType.Header;
            Caption = list.Caption;
            Status = list.Status;
            Id = list.Id;
            HasChildren = !list.Subtree.isEmpty();
        }

        public GridDisplayObject(ItemType type) {
            Type = type;
            Id = null;
        }

        public GridDisplayObject(DisplayObject camera, boolean statusChanged) {
            Type = ItemType.Camera;
            Caption = camera.Caption;
            Status = camera.Status;
            Id = camera.Id;
            Thumbnail = camera.Thumbnail;
        }

        public ItemType Type;
        public boolean NeedsRefresh = false; 
        public boolean HasChildren = false;
    }
    
    private class DummyView extends View {

        public DummyView(Context context) {
            super(context);
        }
        
    }
    
    /**
     * 
     */
    private final Context mContext;
    private final ImageFetcher mImageFetcher;
    private final LayoutInflater mInflater;
    
    private int mItemHeight = 0;
    private int mNumColumns = 0;
    private int mTitleRowHeight = 0;
    private GridView.LayoutParams mImageViewLayoutParams;

    private ArrayList<DisplayObjectList> mSource = new ArrayList<DisplayObjectList>();
    private ArrayList<GridDisplayObject> mData = new ArrayList<GridDisplayObject>();

    public ThumbnailsAdapter(Context context, ImageFetcher imageFetcher) {
        super();
        
        mContext = context;
        mImageFetcher = imageFetcher;        
        mInflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        
        // Calculate Title Bar height
        float px = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 32, context.getResources().getDisplayMetrics());
        mTitleRowHeight = Math.round(px);
        
        mImageViewLayoutParams = new GridView.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    }

    @Override
    public int getCount() {
        return mData.size();
    }

    @Override
    public Object getItem(int position) {
        return mData.get(position);        
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public int getViewTypeCount() {
        return ItemType.values().length;
    }

    @Override
    public int getItemViewType(int position) {
        return mData.get(position).Type.ordinal();
    }

    @Override
    public boolean hasStableIds() {
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup container) {
        
        if (mContext == null)
            return null;
        
        GridDisplayObject displayObject = mData.get(position);
        switch (displayObject.Type) {
        case Camera:
            return getCameraView(convertView, displayObject);
        case Camera_Dummy:
            if (convertView == null || !(convertView instanceof DummyView)) {
                convertView = new DummyView(mContext);
            }
            // Set empty view with height of ImageView
            convertView.setLayoutParams(mImageViewLayoutParams);
            return convertView;
        case Header:
            return getHeaderView(convertView, displayObject);
        case Header_Dummy:
            if (convertView == null || !(convertView instanceof DummyView)) {
                convertView = new DummyView(mContext);
            }
            // Set empty view with height of Title row
            convertView.setLayoutParams(new AbsListView.LayoutParams(1, mTitleRowHeight));
            return convertView;
        }
        return null; //warning supress
    }

    private View getHeaderView(View convertView, GridDisplayObject displayObject) {
        if (convertView == null || !(convertView instanceof LinearLayout)) {
            convertView = mInflater.inflate(R.layout.resource_list_item, null);
        }
        
        TextView t = (TextView) convertView.findViewById(R.id.text_resource);
        t.setText(displayObject.Caption);
        t.setSingleLine();
        t.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        
        ImageView im1 = (ImageView) convertView.findViewById(R.id.icon_resource);
        ImageView imRefresh = (ImageView) convertView.findViewById(R.id.icon_refresh);
        
        if (displayObject.Status == Status.Online && displayObject.HasChildren) {
            imRefresh.setVisibility(View.VISIBLE);
            t.setPadding(8, 8, 48, 0);
        } else {
            imRefresh.setVisibility(View.GONE);
            t.setPadding(8, 8, 0, 0);
        }
        
        switch (displayObject.Status){
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
        
        convertView.setLayoutParams(new AbsListView.LayoutParams(mItemHeight * mNumColumns, mTitleRowHeight));
        convertView.setPadding(32, 0, 0, 0);
        convertView.setBackgroundColor(mContext.getResources().getColor(R.color.grid_title_background));
        return convertView;
    }

    private View getCameraView(View convertView, GridDisplayObject displayObject) {
        if (convertView == null || !(convertView instanceof LinearLayout)) { // if it's not recycled, instantiate and initialize
            convertView = mInflater.inflate(R.layout.resource_grid_item, null);
        } 
        if (mItemHeight <= 0)
            return convertView;
        
        convertView.setLayoutParams(new AbsListView.LayoutParams(mItemHeight, mItemHeight));        
        
        RelativeLayout cameraTitle = (RelativeLayout) convertView.findViewById(R.id.layout_grid_item);
        
        TextView t = (TextView) cameraTitle.findViewById(R.id.text_resource);
        t.setText(displayObject.Caption);
        t.setBackgroundColor(0x77000000);
        
        ImageView imRecording = (ImageView) cameraTitle.findViewById(R.id.icon_recording);
        imRecording.setVisibility(displayObject.Status == Status.Recording ? View.VISIBLE : View.GONE);
        
        ImageView imageView = (ImageView) convertView.findViewById(R.id.image_thumbnail);
        RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams(mItemHeight, mItemHeight);
        lp.setMargins(5, 5, 5, 5);
        imageView.setLayoutParams(lp);
        
        if (displayObject.Status == Status.Offline)
            imageView.setImageResource(R.drawable.thumb_no_signal);
        else if (displayObject.Thumbnail == null || displayObject.Thumbnail.length() == 0) {
            imageView.setImageResource(R.drawable.thumb_loading);
        }        
        else {
            String thmb = displayObject.Thumbnail + "&height="+ mItemHeight;
            // Load the image asynchronously into the ImageView, this also takes care of
            // setting a placeholder image while the background thread runs
            mImageFetcher.loadImage(thmb, imageView, displayObject.NeedsRefresh);
        }
        displayObject.NeedsRefresh = false;
        return convertView;
    }

    /**
     * Sets the item height. Useful for when we know the column width so the height can be set
     * to match.
     *
     * @param height
     */
    public void setItemHeight(int height) {
        if (height < mTitleRowHeight)
            height = mTitleRowHeight;
        
        if (height == mItemHeight) {
            return;
        }
        
        Log("Set item height to " + height);
        mItemHeight = height;
        mImageViewLayoutParams =
                new GridView.LayoutParams(LayoutParams.MATCH_PARENT, mItemHeight);
        mImageFetcher.setImageSize(height);
        notifyDataSetChanged();
    }

    public void setNumColumns(int numColumns) {
        if (mNumColumns == numColumns)
            return;

        Log("Set columns number to " + numColumns);        
        mNumColumns = numColumns;
        
        rebuildData();
        notifyDataSetChanged(); 
    }

    public int getNumColumns() {
        return mNumColumns;
    }

    public void updateData(ArrayList<DisplayObjectList> data) {
        mSource.clear();
        mSource.addAll(data);
        rebuildData();
        notifyDataSetChanged();
    }

    private void rebuildData() {
        
        HashMap<UUID, Status> statuses = new HashMap<UUID, Status>();
        for(GridDisplayObject obj: mData) {
            if (obj.Type == ItemType.Camera_Dummy)
                continue;
            if (obj.Type == ItemType.Header_Dummy)
                continue;            
            statuses.put(obj.Id, obj.Status);
        }
        mData.clear();            
        
        for (DisplayObjectList list: mSource) {
            boolean statusChanged = list.Status != statuses.get(list.Id);
            
            if (mNumColumns > 0) {
                mData.add(new GridDisplayObject(list));
                
                for (int i = 1; i < mNumColumns; i++)
                    mData.add(new GridDisplayObject(ItemType.Header_Dummy));
                
            }
            for (DisplayObject camera : list.Subtree)
                mData.add(new GridDisplayObject(camera, statusChanged || camera.Status != statuses.get(camera.Id)));
            
            if (mNumColumns > 0) {
                while (mData.size() % mNumColumns > 0)
                    mData.add(new GridDisplayObject(ItemType.Camera_Dummy));
            }
        }
        
    }

    public void refreshCameras(int position) {
        int i = position+1;
        while (i < mData.size() && mData.get(i).Type == ItemType.Header_Dummy)
            i++;
        while (i < mData.size() && mData.get(i).Type == ItemType.Camera) {
            mData.get(i).NeedsRefresh = true;
            i++;
        }
        notifyDataSetChanged();
    }
    
    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }
}