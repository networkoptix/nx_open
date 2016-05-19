package com.networkoptix.hdwitness.ui;

import java.util.ArrayList;
import java.util.Collections;

public class DisplayObjectList extends DisplayObject {
	public ArrayList<DisplayObject> Subtree = new ArrayList<DisplayObject>();
	
	public void addItem(DisplayObject item) {

		int index = Collections.binarySearch(Subtree, item);
		
		if (index < 0) {
			Subtree.add(-index-1, item);
		} else{
			Subtree.add(index, item);
		}
			
		return;
	}
}