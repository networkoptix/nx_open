#include "data_copier.h"

bool DataCopier::canAcceptData() const
{
    for (const auto& receptor: m_receptors)
    {
        if (!receptor->canAcceptData())
            return false;
    }

    return true;
}

void DataCopier::putData(const QnAbstractDataPacketPtr& data)
{
    for (const auto& receptor: m_receptors)
        receptor->putData(data);
}

void DataCopier::add(QnAbstractDataReceptorPtr receptor)
{
    m_receptors.push_back(receptor);
}
