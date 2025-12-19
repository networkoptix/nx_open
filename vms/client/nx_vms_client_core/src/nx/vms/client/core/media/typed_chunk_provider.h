// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <nx/utils/impl_ptr.h>

#include "chunk_provider.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API TypedChunkProvider: public QObject
{
    Q_OBJECT

    Q_PROPERTY(ChunkProvider* sourceProvider READ sourceProvider WRITE setSourceProvider
        NOTIFY sourceProviderChanged)
    Q_PROPERTY(Qn::TimePeriodContent contentType READ contentType WRITE setContentType
        NOTIFY contentTypeChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QnTimePeriodList chunks READ chunks NOTIFY chunksChanged)

public:
    explicit TypedChunkProvider(QObject* parent = nullptr);
    virtual ~TypedChunkProvider() override;

    ChunkProvider* sourceProvider() const;
    void setSourceProvider(ChunkProvider* value);

    Qn::TimePeriodContent contentType() const;
    void setContentType(Qn::TimePeriodContent value);

    bool enabled() const;
    void setEnabled(bool value);

    const QnTimePeriodList& chunks() const;

    static void registerQmlType();

signals:
    void sourceProviderChanged();
    void contentTypeChanged();
    void enabledChanged();
    void chunksChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
