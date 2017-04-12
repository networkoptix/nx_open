#pragma once

#include <QtCore/QObject>
#include <nx/utils/singleton.h>
#include <core/resource/resource_factory.h>

namespace nx {

class TestResourceFactory:
    public QObject,
    public QnResourceFactory,
    public Singleton<TestResourceFactory>
{
    Q_OBJECT
public:
    TestResourceFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};

} // namespace nx
