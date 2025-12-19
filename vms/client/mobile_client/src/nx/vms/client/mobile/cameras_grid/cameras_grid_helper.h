// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")

class QnLayoutResource;

namespace nx::vms::client::mobile {

class CamerasGridHelper: public QObject
{
    Q_OBJECT

public:
    static void registerQmlType();

    CamerasGridHelper();
    ~CamerasGridHelper() override;

    Q_INVOKABLE void saveTargetColumnsCount(const QnLayoutResource* layoutResource, int columnsCount);
    Q_INVOKABLE int loadTargetColumnsCount(const QnLayoutResource* layoutResource);

    Q_INVOKABLE void saveLayoutPosition(const QnLayoutResource* layoutResource, int position);
    Q_INVOKABLE int loadLayoutPosition(const QnLayoutResource* layoutResource);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
