// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core
import Nx.Core.Controls

BaseBanner
{
    id: control

    enum Type { Info, Success, Warning, Error }

    property int type: Banner.Info
    property string iconPath: styles[type].iconPath

    textColor: ColorTheme.colors.dark1
    backgroundColor: styles[type].backgroundColor
    closeButtonColor: ColorTheme.colors.dark1
    actionButtonColor: textColor
    borderColor: ColorTheme.colors.dark9
    borderWidth: 1

    readonly property var styles: ({
        [Banner.Info]: {
            backgroundColor: ColorTheme.colors.blue,
            iconPath: "image://skin/24x24/Solid/info.svg",
        },
        [Banner.Success]: {
            backgroundColor: ColorTheme.colors.green_attention,
            iconPath: "image://skin/24x24/Solid/success.svg",
        },
        [Banner.Warning]: {
            backgroundColor: ColorTheme.colors.yellow_attention,
            iconPath: "image://skin/24x24/Solid/warning.svg",
        },
        [Banner.Error]: {
            backgroundColor: ColorTheme.colors.red_attention,
            iconPath: "image://skin/24x24/Solid/error.svg",
        }
    })

    iconItem: ColoredImage
    {
        sourcePath: control.iconPath
        primaryColor: control.textColor
        sourceSize: Qt.size(24, 24)
    }
}
