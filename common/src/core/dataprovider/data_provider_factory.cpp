#include "data_provider_factory.h"

#include <QtCore/QMetaObject>

#include <core/resource/resource.h>

struct QnDataProviderFactory::Private
{
    struct Data
    {
        QByteArray className;
        DataProviderGenerator generator;

        Data(QByteArray&& className, DataProviderGenerator&& generator):
            className(std::move(className)),
            generator(std::move(generator))
        {
        }
    };

    std::list<Data> data;
};

QnDataProviderFactory::QnDataProviderFactory(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

QnDataProviderFactory::~QnDataProviderFactory() = default;

QnAbstractStreamDataProvider* QnDataProviderFactory::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    for (const auto& gen: d->data)
    {
        if (resource->inherits(gen.className))
            return gen.generator(resource, role);
    }
    NX_ASSERT(false, "Data provider is not registered.");
    return nullptr;
}

void QnDataProviderFactory::registerResourceType(
    const QMetaObject& metaobject,
    DataProviderGenerator&& generator)
{
    d->data.emplace_back(metaobject.className(), std::move(generator));
}
