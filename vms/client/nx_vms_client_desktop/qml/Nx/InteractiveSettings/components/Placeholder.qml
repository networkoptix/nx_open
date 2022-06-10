// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import "private"

Placeholder
{
    property string image: "default"
    imageSource: `qrc:///skin/analytics_icons/placeholders/${image}.svg`
    width: parent.width
    height: parent.singleItemHeightHint //< Fill the view height, if it is defined.
}
