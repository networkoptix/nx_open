// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core

TextAreaWithScroll
{
    id: control

    readonly property int kMinLinesNumber: 3
    readonly property int kMaxLinesNumber: 8

    height: MathUtils.bound(
        linesToHeight(kMinLinesNumber),
        textArea.implicitHeight,
        linesToHeight(kMaxLinesNumber))

    wrapMode: TextEdit.Wrap

    textArea.KeyNavigation.priority: KeyNavigation.BeforeItem

    function linesToHeight(linesNumber)
    {
        return textArea.topPadding + textArea.bottomPadding
            + fontMetrics.lineSpacing * (linesNumber - 1) + fontMetrics.height
    }

    FontMetrics
    {
        id: fontMetrics
        font: control.textArea.font
    }
}
