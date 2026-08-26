// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core

import QtQml

NxObject
{
    property string name: ""
    property string title: ""
    property bool enabled: true
    property TutorialTrigger trigger: TutorialTrigger { }
    default property list<TutorialStep> steps
}
