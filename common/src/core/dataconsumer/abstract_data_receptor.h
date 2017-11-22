#pragma once

#include <nx/streaming/media_data_packet.h>

/**
 * Abstract interface of class, accepting media data.
 * NOTE: In a correct way we should make QnAbstractDataConsumer not thread
 *   and add another class derived from QnAbstractDataConsumer.
 */
class QnAbstractDataReceptor
{
public:
    virtual ~QnAbstractDataReceptor() = default;

    /**
     * @return true, if subsequent QnAbstractDataReceptor::putData
     *   call is garanteed to accept input data.
     */
    virtual bool canAcceptData() const = 0;
    /**
     * NOTE: Can ignore data for some reasons (e.g., some internal buffer size is exceeded).
     * Data provider should use canAcceptData method to find out whether it is possible.
     */
    virtual void putData(const QnAbstractDataPacketPtr& data) = 0;
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
};

using QnAbstractMediaDataReceptorPtr = QSharedPointer<QnAbstractMediaDataReceptor>;
