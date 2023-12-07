// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QFont>
#include <QtQml/QQmlPropertyMap>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * Read basic and skin fonts and provide access to them.
 */
class NX_VMS_CLIENT_CORE_API FontConfig: public QQmlPropertyMap
{
    Q_OBJECT
    using base_type = QQmlPropertyMap;

public:
    FontConfig(
        const QFont& baseFont,
        const QString& basicFontsFileName,
        const QString& overrideFontsFileName,
        QObject* parent = nullptr);
    virtual ~FontConfig() override;

    Q_INVOKABLE QFont font(const QString& name) const;

    static void registerQmlType();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
