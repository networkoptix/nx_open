#pragma once

#include <vector>

#include "abstract_data_receptor.h"

/**
 * Copies incoming data stream to multiple receptors.
 */
class DataCopier:
    public QnAbstractDataReceptor
{
public:
    /**
     * @return False, if any of receptor is not ready to accept data.
     */
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    void add(QnAbstractDataReceptorPtr receptor);

private:
    std::vector<QnAbstractDataReceptorPtr> m_receptors;
};
