// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webview_style.h"

#include <QtGui/QPalette>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

QString generateCssStyle()
{
    const auto styleBase = QString::fromLatin1(R"css(
    @font-face {
        font-family: 'Roboto';
        src: url('qrc:///fonts/Roboto-Regular.ttf') format('truetype');
        font-style: normal;
    }
    * {
        color: {windowText};
        font-family: 'Roboto-Regular', 'Roboto';
        font-weight: 600;
        font-size: 13px;
        line-height: 16px;
    }
    body {
        padding-left: 0px;
        margin: 0px;
        overscroll-behavior: none;
        background-color: {window};
    }
    p {
        padding-left: 0px;
    }
    a {
        color: {link};
        font-size: 13px;
    }
    a:hover {
        color: {highlight};
    }
    ::-webkit-scrollbar {
        width: 8px;
    }
    ::-webkit-scrollbar-track {
        background: {scrollbar-track};
    }
    ::-webkit-scrollbar-thumb {
        background: {scrollbar-thumb};
    }
    ::-webkit-scrollbar-thumb:hover {
        background: {scrollbar-thumb-hover};
    }
    )css");

    const auto palette = qApp->palette();

    return QString(styleBase)
        .replace("{windowText}", palette.color(QPalette::WindowText).name())
        .replace("{window}", palette.color(QPalette::Window).name())
        .replace("{link}", palette.color(QPalette::Link).name())
        .replace("{highlight}", palette.color(QPalette::Highlight).name())
        .replace("{scrollbar-track}", colorTheme()->color("dark9").name())
        .replace("{scrollbar-thumb}", colorTheme()->color("dark13").name())
        .replace("{scrollbar-thumb-hover}", colorTheme()->color("dark15").name())
        .simplified();
}

} // namespace nx::vms::client::desktop
