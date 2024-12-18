// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <client/client_globals.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>

class QLabel;

namespace nx::vms::client::desktop {

class Thumbnail: public QWidget
{
    Q_OBJECT

public:
    Thumbnail(const QString& text,
        const QString& iconPath,
        const core::SvgIconColorer::ThemeSubstitutions& theme,
        QWidget* parent = nullptr);
    explicit Thumbnail(Qn::ResourceStatusOverlay overlay, QWidget* parent = nullptr);

    void updateToSize(const QSize& size);

private:
    QLabel* m_iconWidget{};
    QLabel* m_textWidget{};

    void setupUi();
};

} // namespace nx::vms::client::desktop
