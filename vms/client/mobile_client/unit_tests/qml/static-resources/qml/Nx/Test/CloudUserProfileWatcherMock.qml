// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml

/**
 * Mock of the cloud user profile watcher of main.qml, available in QML as
 * `cloudUserProfileWatcher`.
 *
 * Has only what the tested components use. A new member must have the name and the type it has in
 * the CloudUserProfileWatcher class (checked by tst_CloudUserProfileWatcherMock.qml), a writable
 * one is restored in reset().
 */
QtObject
{
    property string fullName

    function reset()
    {
        fullName = ""
    }
}
