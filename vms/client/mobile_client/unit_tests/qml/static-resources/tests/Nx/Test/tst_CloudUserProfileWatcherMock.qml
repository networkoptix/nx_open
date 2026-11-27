// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Test

TestCase
{
    name: "Nx.Test.CloudUserProfileWatcherMock"

    readonly property string kWatcherType: "nx::vms::client::core::CloudUserProfileWatcher"

    function init()
    {
        failOnWarning(/.?/)
    }

    /** Fails for a mock member which CloudUserProfileWatcher does not have. */
    function test_matchesCloudUserProfileWatcher()
    {
        compare(TestUtils.mockMismatches(cloudUserProfileWatcher, kWatcherType).join("\n"), "")
    }
}
