// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {
namespace analytics {

class NX_VMS_CLIENT_DESKTOP_API IconManager: public QObject
{
    Q_OBJECT

public:
    /** @returns Icon file path relative to the skinRootUrl(). Can be used in qnSkin->pixmap(). */
    QString iconPath(const QString& iconName) const;
    QString fallbackIconPath() const;

    /** @returns Absolute icon file location (with "qrc://" protocol prefix). */
    QString absoluteIconPath(const QString& iconName) const;

    /** @returns Absolute icon URL for QML (makes use of SVG image provider). */
    Q_INVOKABLE QString iconUrl(const QString& iconName) const;

    /** The skin root; iconPath() returns paths relative to it. */
    static QString skinRoot();

    /** The root to mount additional libraries to, via QResource::registerResource(). */
    static QString librariesRoot();

    static IconManager* instance();
    static void registerQmlType();

private:
    IconManager();
    virtual ~IconManager() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace analytics
} // namespace nx::vms::client::desktop
