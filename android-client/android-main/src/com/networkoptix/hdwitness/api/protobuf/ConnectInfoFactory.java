package com.networkoptix.hdwitness.api.protobuf;

import java.io.IOException;
import java.io.InputStream;

import com.google.protobuf.implementation.CompatibilityItemProtos.CompatibilityItem;
import com.google.protobuf.implementation.CompatibilityItemProtos.CompatibilityItems;
import com.google.protobuf.implementation.ConnectInfoProtos.ConnectInfo;
import com.networkoptix.hdwitness.api.data.ApiFactoryInterface.ServerInfoFactoryInterface;
import com.networkoptix.hdwitness.api.data.ServerInfo;
import com.networkoptix.hdwitness.common.CompatibilityChecker;
import com.networkoptix.hdwitness.common.SoftwareVersion;


public class ConnectInfoFactory implements ServerInfoFactoryInterface {
    
    public ServerInfo createConnectInfo(InputStream stream, String appVersion) throws IOException {
        ConnectInfo info = ConnectInfo.parseFrom(stream);

        ServerInfo result = new ServerInfo();
        result.Brand = info.hasBrand() ? info.getBrand() : "";
        result.ProxyPort = info.getProxyPort();
        result.Compatible = compatibilityCheck(
                info.hasCompatibilityItems() ? info.getCompatibilityItems()
                        : null, info.hasVersion() ? info.getVersion() : "",
                                appVersion);
        if (info.hasVersion())
            result.Version = new SoftwareVersion(info.getVersion());
        return result;
    }

    

    private static String majorVersion(String version) {
        String[] strings = version.split("\\.");
        return strings.length > 1 ? strings[0] + "." + strings[1] : version;
    }
    
    
    private static boolean compatibilityCheck(CompatibilityItems items, String ecVersion, String appVersion) {

        /**
         * Flag set to true if server knows about this version of the android
         * client
         */
        boolean serverCheckSuccess = false;

        String ecMajor = majorVersion(ecVersion);

        if (items != null) {
            for (int i = 0; i < items.getItemCount(); i++) {
                CompatibilityItem item = items.getItem(i);
                String app = item.hasComp1() ? item.getComp1() : "";

                /**
                 * Ignore strings for another projects
                 */
                if (!app.equals("android"))
                    continue;

                /**
                 * Ignore strings for other versions of android app
                 */
                String appVer = item.hasVer1() ? item.getVer1() : "";
                if (appVer.length() == 0 || !appVersion.startsWith(appVer))
                    continue;

                /**
                 * If at least one string with current version is found, check
                 * if versions of an android app and EC are equal
                 */
                if (!serverCheckSuccess) {
                    serverCheckSuccess = true;
                    if (majorVersion(appVersion).equals(ecMajor))
                        return true;
                }

                String compatVer = item.hasVer2() ? item.getVer2() : "";
                if (majorVersion(compatVer).equals(majorVersion(ecVersion)))
                    return true;
            }
        }

        if (serverCheckSuccess)
            return false;
        return CompatibilityChecker.localCompatibilityCheck(ecVersion);
    }
    
    
}
