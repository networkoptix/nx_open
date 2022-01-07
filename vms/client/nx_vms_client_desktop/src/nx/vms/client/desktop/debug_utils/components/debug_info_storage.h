// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>
#include <QString>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>


namespace nx::vms::client::desktop {

 /**
 * Storage of custom debug-level information, which can be used from everywhere in the client code.
 */
class DebugInfoStorage: public QObject, public Singleton<DebugInfoStorage>
{
    Q_OBJECT
    using base_type = QObject;

public:
    DebugInfoStorage(QObject* parent = nullptr);
    virtual ~DebugInfoStorage() override;

    void setValue(const QString& key, const QString& value);
    void removeValue(const QString& key);
    const QStringList& debugInfo() const;

signals:
    void debugInfoChanged(const QStringList& debugInfo);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
