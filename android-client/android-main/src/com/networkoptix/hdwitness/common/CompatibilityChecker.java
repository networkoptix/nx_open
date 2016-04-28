package com.networkoptix.hdwitness.common;

public class CompatibilityChecker {
      
    /**
     * Check server version compatibility for servers lower than 1.6
     * 
     * @param version
     *            - current server version.
     * @return true if server version is equal or greater than 1.3.1.0 and less than 2.3.0
     */
    public static boolean localCompatibilityCheck(String versionString) {
        SoftwareVersion version = new SoftwareVersion(versionString);
        if (!version.isValid())
            return false;
        
        SoftwareVersion oldestAllowed = new SoftwareVersion(1, 3, 1);
        if (version.lessThan(oldestAllowed))
            return false;
               
        return true;
    }
    
}
