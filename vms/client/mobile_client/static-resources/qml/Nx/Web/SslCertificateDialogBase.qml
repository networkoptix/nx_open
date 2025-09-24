// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Popups 1.0

PopupBase
{
    id: control

    property string result

    property string messageText
    property string adviceText

    property string certificateCommonName
    property string certificateIssuedBy
    property string certificateExpires
    property string certificateSHA256
    property string certificateSHA1

    autoClose: false
    messages: [
        messageText,
        adviceText,
        "%1: %2\n%3: %4\n%5: %6".arg(qsTr("Common name")).arg(certificateCommonName)
            .arg(qsTr("Issued by")).arg(certificateIssuedBy)
            .arg(qsTr("Expires")).arg(certificateExpires),
        "%1\n%2\n%3".arg(qsTr("Fingerprints")).arg(certificateSHA256).arg(certificateSHA1)]
}
