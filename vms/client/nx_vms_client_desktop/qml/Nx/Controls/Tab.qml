// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

QtObject
{
    property Item button //< Tab button.

    default property Item page //< Content page.

    property bool visible: true //< Whether false, the tab is temporarily removed.
}
