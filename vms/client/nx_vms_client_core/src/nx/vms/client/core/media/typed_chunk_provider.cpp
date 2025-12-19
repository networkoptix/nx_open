// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "typed_chunk_provider.h"

#include <QtCore/QPointer>
#include <QtQml/QtQml>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::core {

struct TypedChunkProvider::Private
{
    QPointer<ChunkProvider> sourceProvider;
    nx::utils::ScopedConnections connections;
    Qn::TimePeriodContent contentType = Qn::TimePeriodContent::RecordingContent;
    bool enabled = true;
};

TypedChunkProvider::TypedChunkProvider(QObject* parent):
    QObject(parent),
    d(new Private{})
{
}

TypedChunkProvider::~TypedChunkProvider()
{
    // Required here for forward-declared scoped pointer destruction.
}

ChunkProvider* TypedChunkProvider::sourceProvider() const
{
    return d->sourceProvider;
}

void TypedChunkProvider::setSourceProvider(ChunkProvider* value)
{
    if (d->sourceProvider == value)
        return;

    d->connections.reset();
    d->sourceProvider = value;

    if (d->sourceProvider)
    {
        d->connections << connect(d->sourceProvider, &ChunkProvider::periodsUpdated, this,
            [this](Qn::TimePeriodContent updatedContentType)
            {
                if (d->enabled && updatedContentType == d->contentType)
                    emit chunksChanged();
            });

        d->connections << connect(d->sourceProvider, &QObject::destroyed, this,
            [this]()
            {
                if (d->enabled)
                    emit chunksChanged();
            });
    }

    emit sourceProviderChanged();

    if (d->enabled)
        emit chunksChanged();
}

Qn::TimePeriodContent TypedChunkProvider::contentType() const
{
    return d->contentType;
}

void TypedChunkProvider::setContentType(Qn::TimePeriodContent value)
{
    if (d->contentType == value)
        return;

    d->contentType = value;
    emit contentTypeChanged();

    if (d->enabled)
        emit chunksChanged();
}

bool TypedChunkProvider::enabled() const
{
    return d->enabled;
}

void TypedChunkProvider::setEnabled(bool value)
{
    if (d->enabled == value)
        return;

    d->enabled = value;
    emit enabledChanged();

    emit chunksChanged();
}

const QnTimePeriodList& TypedChunkProvider::chunks() const
{
    static const QnTimePeriodList kEmptyTimePeriodList;

    return d->sourceProvider && d->enabled
        ? d->sourceProvider->periods(d->contentType)
        : kEmptyTimePeriodList;
}

void TypedChunkProvider::registerQmlType()
{
    qmlRegisterType<TypedChunkProvider>("nx.vms.client.core", 1, 0, "TypedChunkProvider");
}

} // namespace nx::vms::client::core
