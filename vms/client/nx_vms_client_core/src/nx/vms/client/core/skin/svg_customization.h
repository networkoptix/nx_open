// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QColor>

namespace nx::vms::client::core {

NX_VMS_CLIENT_CORE_API QByteArray customizeSvgColorClasses(const QByteArray& sourceData,
    const QMap<QString /*source class name*/, QColor /*target color*/>& customization,
    const QString& svgName, //< for debug output, a meaningful value must be passed.
    QSet<QString>* classesMissingCustomization = nullptr);

[[deprecated]]
NX_VMS_CLIENT_CORE_API QByteArray substituteSvgColors(const QByteArray& sourceData,
    const QMap<QColor /*source color*/, QColor /*target color*/>& substitutions);

} // namespace nx::vms::client::core
