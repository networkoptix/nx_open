// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

namespace nx::vms::client::mobile {

class WindowContext;

class QmlWrapperHelper
{
public:
    static QString showPopup(
        WindowContext* context,
        const QUrl& source,
        QVariantMap properties = QVariantMap());

    static QString showScreen(
        WindowContext* context,
        const QUrl& source,
        const QVariantMap& properties = QVariantMap());

    /** Pushes the screen without waiting for its result. Returns the created screen. */
    static QObject* pushScreen(
        WindowContext* context, const QUrl& source, const QVariantMap& properties = QVariantMap());
};

} // namespace nx::vms::client::mobile
