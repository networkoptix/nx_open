package com.networkoptix.hdwitness.api.resources;

import java.util.UUID;

public class UserResource extends AbstractResource {
    private static final long serialVersionUID = 4071120082280630431L;

    private boolean mIsAdmin;
    private long mPermissions;
    private String mRealm = "NetworkOptix"; /* v2.3 compatibility */

    /** Can view all cameras */
    public static final long GlobalAdministratorPermission          = 0x00000002L;
    
    /** Can view archives of available cameras. */
    public static final long GlobalViewArchivePermission            = 0x00000100L;
    
    /** Can view archives of available cameras - deprecated value, left for compatibility. */
    public static final long DeprecatedViewExportArchivePermission  = 0x00000040L;

    public UserResource(UUID id) {
        super(id, ResourceType.User);
    }

    public void setAdmin(boolean isAdmin) {
        mIsAdmin = isAdmin;
    }

    public boolean isAdmin() {
        return mIsAdmin;
    }
    
    public String getRealm() {
        return mRealm;
    }
    
    public void setRealm(String realm) {
        if (realm != null && realm.length() > 0)
            mRealm = realm;
    }

    public void setPermissions(long permissions) {
        mPermissions = permissions;
    }

    public long getPermissions() {
        return mPermissions;
    }

    public long getEffectivePermissions() {
        if (!mIsAdmin)
            return mPermissions;
        return Long.MAX_VALUE;
    }

    public boolean hasPermissions(long permissions) {
        return (getEffectivePermissions() & permissions) == permissions;
    }
    
    @Override
    public void update(AbstractResource resource) {
        if (!(resource instanceof UserResource))
            throw new IllegalStateException();
        UserResource user = (UserResource) resource;
        mIsAdmin = user.isAdmin();
        mPermissions = user.getPermissions();
        setRealm(user.getRealm());

        super.update(resource);
    }

}
