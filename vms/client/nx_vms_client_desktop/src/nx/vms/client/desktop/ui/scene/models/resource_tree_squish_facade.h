// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

class ResourceTreeModelAdapter;

class ResourceTreeModelSquishFacade: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString jsonModel READ jsonModel)

public:
    ResourceTreeModelSquishFacade(ResourceTreeModelAdapter* adapter);

    QString jsonModel();

private:
    const ResourceTreeModelAdapter* m_adapter = nullptr;
};

} // namespace nx::vms::client::desktop
