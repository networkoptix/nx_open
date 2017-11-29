#include "metadata_multiplexer.h"

QnAbstractCompressedMetadataPtr MetadataMultiplexer::getMotionData(qint64 timeUsec)
{
    ReaderContext* selectedReaderContext = nullptr;
    for (auto& readerContext: m_readers)
    {
        if (!readerContext.packet)
            readerContext.packet = readerContext.reader->getMotionData(timeUsec);

        if (!readerContext.packet)
            continue;

        if (!selectedReaderContext ||
            readerContext.packet->timestamp < selectedReaderContext->packet->timestamp)
        {
            selectedReaderContext = &readerContext;
        }
    }

    QnAbstractCompressedMetadataPtr result;
    if (selectedReaderContext)
        std::swap(result, selectedReaderContext->packet);

    return result;
}

void MetadataMultiplexer::add(QnAbstractMotionArchiveConnectionPtr metadataReader)
{
    m_readers.emplace_back(ReaderContext());
    m_readers.back().reader = std::move(metadataReader);
}
