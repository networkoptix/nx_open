// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls

DialogBanner
{
    required property string login
    property bool isLoginLowercase: login.toLowerCase() === login

    style: isLoginLowercase ? DialogBanner.Style.Warning : DialogBanner.Style.Error
    closeable: false

    text: isLoginLowercase
        ? qsTr("Digest authentication is deprecated and will be disabled in the next " +
            "release, and should only be used when default Bearer Authentication " +
            "cannot be used.")
        : qsTr("Digest authentication is deprecated and will be disabled in the next " +
            "release, and should only be used when default Bearer Authentication " +
            "cannot be used. User logins must consist only of lowercase letters.")
}
