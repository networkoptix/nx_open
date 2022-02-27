// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::desktop {

class SvgIconProvider: public QQuickImageProvider
{
public:
    SvgIconProvider();

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;
};

} // namespace nx::vms::client::desktop
