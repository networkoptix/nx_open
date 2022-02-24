// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import "private"

Figure
{
    figureType: "line"

    property string allowedDirections: ""
    property int minPoints: 2
    property int maxPoints: 0

    figureSettings:
    {
        "minPoints": minPoints,
        "maxPoints": maxPoints,
        "allowedDirections": allowedDirections
    }
}
