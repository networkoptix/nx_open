// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>

namespace nx::vms::client::core {

NX_VMS_CLIENT_CORE_API QPixmap loadSvgImage(const QString& sourcePath,
    const QMap<QString /*source class name*/, QColor /*target color*/>& customization,
    const QSize& desiredPhysicalSize = QSize{},
    const qreal devicePixelRatio = 1.0);

NX_VMS_CLIENT_CORE_API QPixmap loadSvgImage(const QString& sourcePath,
    const QMap<QString /*source class name*/, QString /*target color name*/>& customization,
    const QSize& desiredPhysicalSize = QSize{},
    const qreal devicePixelRatio = 1.0);

} // namespace nx::vms::client::core
