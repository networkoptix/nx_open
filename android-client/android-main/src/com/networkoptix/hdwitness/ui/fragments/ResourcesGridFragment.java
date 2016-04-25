package com.networkoptix.hdwitness.ui.fragments;

import java.util.ArrayList;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.GridView;
import android.widget.Toast;

import com.networkoptix.hdwitness.BuildConfig;
import com.networkoptix.hdwitness.R;
import com.networkoptix.hdwitness.api.data.SessionInfo;
import com.networkoptix.hdwitness.common.Utils;
import com.networkoptix.hdwitness.common.image.ImageCache.ImageCacheParams;
import com.networkoptix.hdwitness.common.image.ImageFetcher;
import com.networkoptix.hdwitness.ui.DisplayObject;
import com.networkoptix.hdwitness.ui.DisplayObjectList;
import com.networkoptix.hdwitness.ui.activities.HdwActivity;
import com.networkoptix.hdwitness.ui.adapters.ThumbnailsAdapter;

/**
 * The main fragment that powers the ImageGridActivity screen. Fairly straight forward GridView implementation with the key addition being the
 * ImageWorker class w/ImageCache to load children asynchronously, keeping the UI nice and smooth and caching thumbnails for quick retrieval. The
 * cache is retained over configuration changes like orientation change so the images are populated quickly if, for example, the user rotates the
 * device.
 */
public class ResourcesGridFragment extends AbstractResourcesFragment {
    private static final String IMAGE_CACHE_DIR = "thumbs";

    private int mImageThumbSize;
    private int mImageThumbSpacing;
    private int mDisplayWidth = 0;    
    
    private ThumbnailsAdapter mAdapter;
    private GridView mGridView;
    ImageFetcher mImageFetcher;

    /**
     * Empty constructor as per the Fragment documentation
     */
    public ResourcesGridFragment() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);

        mImageThumbSize = getResources().getDimensionPixelSize(R.dimen.image_thumbnail_size);
        mImageThumbSpacing = getResources().getDimensionPixelSize(R.dimen.image_thumbnail_spacing);
        Log("Initializing grid with thumbnail widht " + mImageThumbSize);
        Log("Initializing grid with spacing " + mImageThumbSpacing);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View v = inflater.inflate(R.layout.fragment_resources_grid, container, false);
        mGridView = (GridView) v.findViewById(R.id.gridView);
        return v;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        ImageCacheParams cacheParams = new ImageCacheParams(getActivity(), IMAGE_CACHE_DIR);

        // Set memory cache to 25% of mem class
        cacheParams.setMemCacheSizePercent(getActivity(), 0.25f);

        SessionInfo sessionInfo = ((HdwActivity) getActivity()).getApp().getSessionInfo();
        if (sessionInfo == null)
            return;

        // The ImageFetcher takes care of loading images into our ImageView children asynchronously
        mImageFetcher = new ImageFetcher(getActivity(), mImageThumbSize, sessionInfo.getCustomHeaders());
        if (sessionInfo.getCustomHeaders().isEmpty())
            throw new AssertionError();
        mImageFetcher.setLoadingImage(R.drawable.thumb_loading);
        mImageFetcher.setNotAvailableImage(R.drawable.thumb_no_data);
        mImageFetcher.addImageCache(getActivity().getSupportFragmentManager(), cacheParams);

        mAdapter = new ThumbnailsAdapter(getActivity(), mImageFetcher);
        mGridView.setAdapter(mAdapter);

        mGridView.setOnItemClickListener(new OnItemClickListener() {

            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                DisplayObject cameraObj = (DisplayObject) mAdapter.getItem(position);

                ThumbnailsAdapter.ItemType itemType = ThumbnailsAdapter.ItemType.values()[mAdapter.getItemViewType(position)];
                switch (itemType) {
                case Camera:
                    openCamera(v, cameraObj.Id);
                    break;
                case Header:
                    mAdapter.refreshCameras(position);
                    break;
                default:
                    break;
                }
            }

        });

        mGridView.setOnScrollListener(new AbsListView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(AbsListView absListView, int scrollState) {
                // Pause fetcher to ensure smoother scrolling when flinging
                if (mImageFetcher == null)
                    return;

                if (scrollState == AbsListView.OnScrollListener.SCROLL_STATE_FLING) {
                    mImageFetcher.setPauseWork(true);
                } else {
                    mImageFetcher.setPauseWork(false);
                }
            }

            @Override
            public void onScroll(AbsListView absListView, int firstVisibleItem, int visibleItemCount, int totalItemCount) {
            }
        });
        
        initLayoutListeners();
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    private void initLayoutListeners() {
        if (Utils.hasHoneycomb()) {
            mGridView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    updateLayout();
                }
            });
        }

        // This listener is used to get the final width of the GridView and then calculate the number of columns and the width of each column. The
        // width of each column is variable as the GridView has stretchMode=columnWidth. The column width is used to set the height of each view so we
        // get nice square thumbnails.
        mGridView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                updateLayout();
            }
        });
    }

    private void updateLayout() {       
        if (mDisplayWidth == mGridView.getWidth())
            return;
        
        mDisplayWidth = mGridView.getWidth();
        Log("Updating layout. Display width " + mDisplayWidth);

        /* For N columns we use N+1 spacers. */
        final double width = mDisplayWidth - mImageThumbSpacing;        

        final int numColumns = (int) Math.floor(width / (mImageThumbSize + mImageThumbSpacing));

        if (numColumns > 0) {
            final int columnWidth = (int) Math.floor(width / numColumns) - mImageThumbSpacing;
            mAdapter.setItemHeight(columnWidth);            
            mAdapter.setNumColumns(numColumns);
            mGridView.setNumColumns(numColumns);
        } else {
            Log("Ignoring invalid calculations");
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mImageFetcher == null)
            return;
        mImageFetcher.setExitTasksEarly(false);
    }

    @Override
    public void onPause() {
        super.onPause();

        if (mImageFetcher == null)
            return;
        mImageFetcher.setExitTasksEarly(true);
        mImageFetcher.flushCache();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mImageFetcher == null)
            return;
        mImageFetcher.closeCache();
    }

    public void clearCache() {
        if (mImageFetcher == null)
            return;
        mImageFetcher.clearCache();
        Toast.makeText(getActivity(), R.string.clear_cache_complete_toast, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.menu_resources_grid, menu);
        super.onCreateOptionsMenu(menu, inflater);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case R.id.menu_clear_cache:
            clearCache();
            break;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void updateData(ArrayList<DisplayObjectList> data) {
        mAdapter.updateData(data);
        mAdapter.notifyDataSetChanged();
    }

    private void Log(String message) {
        if (!BuildConfig.DEBUG)
            return;
        Log.d(this.getClass().getSimpleName(), message);
    }
}
