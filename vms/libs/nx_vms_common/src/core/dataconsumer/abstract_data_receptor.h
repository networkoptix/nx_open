// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/media/media_data_packet.h>

/**
 * Abstract interface of class, accepting media data.
 * NOTE: In a correct way we should make QnAbstractDataConsumer not thread
 *   and add another class derived from QnAbstractDataConsumer.
 */
class NX_VMS_COMMON_API QnAbstractDataReceptor
{
public:
    virtual ~QnAbstractDataReceptor();

    /**
     * @return true, if subsequent QnAbstractDataReceptor::putData
     *   call is guaranteed to accept input data.
     */
    virtual bool canAcceptData() const = 0;

    /**
     * NOTE: Can ignore data for some reasons (e.g., some internal buffer size is exceeded).
     * Data provider should use canAcceptData method to find out whether it is possible.
     */
    virtual void putData(const QnAbstractDataPacketPtr& data) = 0;

    virtual void clearUnprocessedData() = 0;

    /** Sanity check. */
    std::atomic<size_t> consumers{0};
};

using QnAbstractDataReceptorPtr = QSharedPointer<QnAbstractDataReceptor>;

class QnAbstractMediaDataReceptor:
    public QnAbstractDataReceptor
{
public:
    /**
     * DataReceptor is required that provider should be fully configured.
     */
    virtual bool needConfigureProvider() const { return true; }

    /**
     * Allow to replace video data if Tier limit overused.
     */
    virtual bool canReplaceData() const { return true; }
};

using QnAbstractMediaDataReceptorPtr = QSharedPointer<QnAbstractMediaDataReceptor>;
