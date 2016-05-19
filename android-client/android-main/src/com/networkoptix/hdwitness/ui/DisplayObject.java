package com.networkoptix.hdwitness.ui;

import java.util.UUID;

import com.networkoptix.hdwitness.api.resources.Status;

public class DisplayObject implements Comparable<DisplayObject>{
	public String Caption;
	public Status Status;
	public UUID Id;
    public String Thumbnail;
	
	public int compareTo(DisplayObject another) {
		return this.Caption.compareTo(another.Caption); 
	}
}