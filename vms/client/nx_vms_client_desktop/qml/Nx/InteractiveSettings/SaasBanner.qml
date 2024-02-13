// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls

DialogBanner
{
    id: root

    required property var saasServiceManager
    required property var licenseSummary
    required property bool deviceSpecific

    visible: !!text && !!licenseSummary

    text:
    {
        if (!saasServiceManager)
            return ""

        if (saasServiceManager.saasSuspended)
        {
            return deviceSpecific
                ? qsTr("Site has been suspended. To enable/disable the integration usage for"
                    + " current device the Site must be in active state. Contact your channel"
                    + " partner for details")
                : qsTr("Site has been suspended. To enable/disable the integration usage for"
                    + " some device the Site must be in active state. Contact your channel"
                    + " partner for details")
        }

        if (saasServiceManager.saasShutDown)
        {
            return deviceSpecific
                ? qsTr("Site has been shut down. To enable this integration usage for current"
                    + " device the Site must be in active state. Contact your channel partner"
                    + " for details")
                : qsTr("Site has been shut down. To activate this integration usage for some"
                    + " device the Site must be in active state. Contact your channel partner"
                    + " for details")
        }

        return ""
    }
}
