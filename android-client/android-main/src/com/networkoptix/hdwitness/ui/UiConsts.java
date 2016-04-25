package com.networkoptix.hdwitness.ui;


/**
 * Public ui constants storage.
 * All strings are starts with "_" to avoid collisions with private constants.
 * All integers are based on 1000 because of the same reason.
 */
public final class UiConsts {
	
	public static final String ARCHIVE_CHUNKS = "_archive_chunks";
	public static final String ARCHIVE_POSITION = "_archive_position";

	/**
	 * Broadcast intent for updating resources list.
	 */
	public static final String BCAST_RESOURCES_UPDATED = "com.networkoptix.hdwitness.RESOURCES_UPDATE_ACTION";
	
	   /**
     * Broadcast intent for forced logoff.
     */
    public static final String BCAST_LOGOFF = "com.networkoptix.hdwitness.LOGOFF_ACTION";
	
	/**
     * Broadcast intent for updating "Show offline" flag.
     */
	public static final String BCAST_SHOW_OFFLINE_UPDATED = "com.networkoptix.hdwitness.SHOW_OFFLINE_UPDATE_ACTION";
	
	public static final int ARCHIVE_POSITION_REQUEST = 1001;

}
