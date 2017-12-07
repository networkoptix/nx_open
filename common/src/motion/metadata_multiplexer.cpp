#include "metadata_multiplexer.h"

QnAbstractCompressedMetadataPtr MetadataMultiplexer::getMotionData(qint64 timeUsec)
{
    ReaderContext* selectedReaderContext = nullptr;
    for (auto& readerIdAndContext: m_readers)
    {
        auto& readerContext = readerIdAndContext.second;

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

void MetadataMultiplexer::add(
    int id,
    QnAbstractMotionArchiveConnectionPtr metadataReader)
{
    ReaderContext context;
    context.reader = std::move(metadataReader);
    m_readers[id] = std::move(context);
}

QnAbstractMotionArchiveConnectionPtr MetadataMultiplexer::readerById(int id)
{
    auto it = m_readers.find(id);
    return it == m_readers.end() ? nullptr : it->second.reader;
}

void MetadataMultiplexer::removeById(int id)
{
    m_readers.erase(id);
}
