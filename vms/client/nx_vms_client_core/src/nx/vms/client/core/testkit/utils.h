// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQml/QJSValue>
#include <QtCore/QRect>

class QWindow;

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

/**
 * Takes screenshot of the primary screen (or main window on iOS/Android) and returns its image
 * data. Screenshot is scaled down by application pixel ratio so coordinates inside the image are
 * directly mapped to top level widgets.
 * Format name is one of the supported Qt formats for writing, see
 * https://doc.qt.io/qt-6/qpixmap.html#reading-and-writing-image-files
 */
NX_VMS_CLIENT_CORE_API QByteArray takeScreenshot(const char* format = "PNG");

QVariantList findAllObjects(QJSValue properties);

/** Returns objectName() for QObject or QML id and source location for QQuickItem. */
NX_VMS_CLIENT_CORE_API std::pair<QString, QString> nameAndBaseUrl(const QObject* object);

/** Returns QObject properties as QVarintMap. */
QVariant dumpQObject(const QObject* object, bool withChildren = false);

enum KeyOption {
    KeyType = 0,
    KeyPress = 1,
    KeyRelease = 2,
};

/**
 * Types specified text into the object.
 * Also supports sequences like `<Enter>`, `<Ctrl+S>` etc.
 */
NX_VMS_CLIENT_CORE_API Qt::KeyboardModifiers sendKeys(
    QString keys,
    QObject* receiver = nullptr,
    KeyOption option = KeyType,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier);

/**
 * Sends mouse event(s) to the specified window.
 * @param button Button that caused the event. If the event type is MouseMove, the appropriate button for this event is Qt::NoButton.
 * @param buttons State of all buttons at the time of the event.
 * @param modifiers State of all keyboard modifiers.
 */
NX_VMS_CLIENT_CORE_API Qt::MouseButtons sendMouse(
    QPoint screenPos,
    QString eventType,
    Qt::MouseButton button,
    Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers,
    QWindow* windowHandle = nullptr,
    bool nativeSetPos = false,
    QPoint pixelDelta = {},
    QPoint angleDelta = {},
    bool inverted = false,
    int scrollDelta = 0);

} // namespace nx::vms::client::core::testkit::utils
