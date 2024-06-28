// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQml/QJSValue>
#include <QtCore/QRect>

namespace nx::vms::client::core::testkit::utils {

/** Returns true if text from UI element (with/without shotcuts) matches with specified text. */
NX_VMS_CLIENT_CORE_API bool textMatches(QString itemText, QString text);

/** Returns true if object properties match with specified properties. */
NX_VMS_CLIENT_CORE_API bool objectMatches(const QObject* object, QJSValue properties);

/** Property type or method return type info. */
NX_VMS_CLIENT_CORE_API QVariantMap getMetaInfo(const QMetaObject* meta, QString methodOrProperty);

/**
 * Returns a screen rectangle of QQuickItem.
 * Returns invalid QRect if the provided object does not have a proper type.
 */
QRect globalRect(QVariant object);

QVariantList findAllObjects(QJSValue properties);

/** Returns objectName() for QObject or QML id and source location for QQuickItem. */
NX_VMS_CLIENT_CORE_API std::pair<QString, QString> nameAndBaseUrl(const QObject* object);

/** Returns QObject properties as QVarintMap. */
QVariant dumpQObject(const QObject* object, bool withChildren = false);

} // namespace nx::vms::client::core::testkit::utils
