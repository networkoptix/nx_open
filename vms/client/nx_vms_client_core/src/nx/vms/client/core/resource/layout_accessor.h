// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>


namespace nx::vms::client::core {
namespace resource {

class NX_VMS_CLIENT_CORE_API LayoutAccessor:
    public QObject,
    public CommonModuleAware
{
    Q_OBJECT
    Q_PROPERTY(QString layoutId READ layoutId WRITE setLayoutId NOTIFY layoutChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)

    using base_type = QObject;

public:
    LayoutAccessor(QObject* parent = nullptr);
    ~LayoutAccessor();

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QnLayoutResourcePtr layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    QString name() const;

signals:
    void layoutAboutToBeChanged();
    void layoutChanged();
    void nameChanged();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace resource
} // namespace nx::vms::client::core
