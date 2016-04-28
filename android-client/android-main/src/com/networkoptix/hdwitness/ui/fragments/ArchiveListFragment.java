package com.networkoptix.hdwitness.ui.fragments;

import java.util.ArrayList;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.app.ListFragment;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.networkoptix.hdwitness.api.data.ChunkList;
import com.networkoptix.hdwitness.ui.UiConsts;

public abstract class ArchiveListFragment extends ListFragment {

	private static final String STORED_CHILD_INDEX = "child_index";
	private static final String STORED_INDEX = "index";
	private static final String STORED_TITLES = "titles";
	private static final String STORED_CHUNKS = "chunkslists";
	
	
	protected boolean mChildAsFrame;
	protected int mChildIndex = 0;
	protected ArrayList<String> mTitles = new ArrayList<String>();
	protected ArrayList <ChunkList> mChunkLists = new ArrayList<ChunkList>();
	
	
    @Override
    @SuppressWarnings("unchecked")
	public void onActivityCreated(Bundle savedInstanceState) {
	    super.onActivityCreated(savedInstanceState);
	    
	    if (savedInstanceState != null){
	    	mTitles = savedInstanceState.getStringArrayList(STORED_TITLES);
	    	mChunkLists = (ArrayList<ChunkList>) savedInstanceState.getSerializable(STORED_CHUNKS);
	    } else
	    	generateChunksAndTitles((ChunkList) getArguments().getSerializable(UiConsts.ARCHIVE_CHUNKS));
	    
	    setListAdapter(new ArrayAdapter<String>(getActivity(),
	            android.R.layout.simple_list_item_1, mTitles));
	
	    View childFrame = getChildId() > 0 ? getActivity().findViewById(getChildId()) : null;
	    mChildAsFrame = childFrame != null && childFrame.getVisibility() == View.VISIBLE;
	    
	    if (savedInstanceState != null) {
	        // Restore last state for checked position.
	        mChildIndex = savedInstanceState.getInt(STORED_CHILD_INDEX, 0);
	    }
	
	    if (mChildAsFrame) {
	        // In dual-pane mode, the list view highlights the selected item.
	        getListView().setChoiceMode(ListView.CHOICE_MODE_SINGLE);
	        // Make sure our UI is in the correct state.
	        showItems(mChildIndex);
	    }
	}

	@Override
	public void onSaveInstanceState(Bundle outState) {
	    super.onSaveInstanceState(outState);
	    outState.putInt(STORED_CHILD_INDEX, mChildIndex);
	    outState.putStringArrayList(STORED_TITLES, mTitles);
	    outState.putSerializable(STORED_CHUNKS, mChunkLists);
	}

	@Override
	public void onListItemClick(ListView l, View v, int position, long id) {
		showItems(position);
	}
	

	public int getShownIndex() {
		return getArguments().getInt(STORED_INDEX, 0);
	}
	
    /**
     * Helper function to show the details of a selected item, either by
     * displaying a fragment in-place in the current UI, or starting a
     * whole new activity in which it is displayed.
     */
	protected void showItems(int index){
        mChildIndex = index;

        if (mChildAsFrame) {
            // We can display everything in-place with fragments, so update
            // the list to highlight the selected item and show the data.
            getListView().setItemChecked(index, true);

            // Check what fragment is currently shown, replace if needed.
            ArchiveListFragment child = (ArchiveListFragment) getFragmentManager().findFragmentById(getChildId());
            if (child == null || child.getShownIndex() != index) {
                // Make new fragment to show this selection.
                child = getChild();
                Bundle args = new Bundle();
                args.putInt(STORED_INDEX, index);
                args.putSerializable(UiConsts.ARCHIVE_CHUNKS, mChunkLists.get(index));
                child.setArguments(args);

                // Execute a transaction, replacing any existing fragment
                // with this one inside the frame.
                FragmentTransaction ft = getFragmentManager().beginTransaction();
                ft.replace(getChildId(), child);
                ft.setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE);
                ft.commit();
            }

        } else {
            // Otherwise we need to launch a new activity to display child view
            Intent intent = new Intent();
            intent.setClass(getActivity(), getChildActivityClass());
            intent.putExtra(STORED_INDEX, index);
            intent.putExtra(UiConsts.ARCHIVE_CHUNKS, mChunkLists.get(index));
            startActivityForResult(intent, UiConsts.ARCHIVE_POSITION_REQUEST);
        }
	}

	protected Class<?> getChildActivityClass() {
		return null;
	}

	protected Class<? extends ArchiveListFragment> getChildClass(){
		return null;
	}

	protected ArchiveListFragment getChild() {
		Class<? extends ArchiveListFragment> c = getChildClass();
		if (c != null)
			try {
				return c.newInstance();
			} catch (java.lang.InstantiationException e) {
				e.printStackTrace();
			} catch (IllegalAccessException e) {
				e.printStackTrace();
			}
		return null;
	}

	protected int getChildId(){
		return 0;
	}
	
	protected void generateChunksAndTitles(ChunkList chunks){
	}

}