#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <utils/memory/cycle_buffer.h>

bool isDataEq(const QByteArray& data, char ch)
{
    for(int i = 0; i < data.size(); ++i) 
    {
        if (data[i] != ch)
            return false;
    }
    return true;
}

void checkTestPattern(QnMediaCyclicBuffer& buffer)
{
    QByteArray testString("TEST_DATA");
    for (int i = 1; i < buffer.size() - testString.size() - 1; ++i)
    {
        buffer.insert(0, "B", 1);
        buffer.insert(buffer.size()-1, "E", 1);
        buffer.insert(i, testString.data(), testString.size());
        const char* dstData = buffer.unfragmentedData(i, testString.size());
        QByteArray resultData = QByteArray::fromRawData(dstData, testString.size());
        ASSERT_EQ(testString, resultData);
        ASSERT_EQ(*buffer.unfragmentedData(0, 1), 'B');
        ASSERT_EQ(*buffer.unfragmentedData(buffer.size()-1, 1), 'E');
    }
}

TEST( QnMediaCycleBuffer, main )
{
    static const int DATA_BLOCK_SIZE = 32;
    static const int DATA_BLOCKS = 5;

    QnMediaCyclicBuffer buffer(DATA_BLOCK_SIZE * DATA_BLOCKS * 16, 64);

    char testData[DATA_BLOCKS][DATA_BLOCK_SIZE];
    for (int i = 0; i < 5; ++i)
        memset(testData[i], i, DATA_BLOCK_SIZE);

    for (int i = 0; i < 16; ++i) {
        for (int i = 0; i < DATA_BLOCKS; ++i)
            buffer.push_back(testData[i], DATA_BLOCK_SIZE);
    }
    for (int i = 0; i < DATA_BLOCKS; ++i) {
        buffer.pop_front(DATA_BLOCK_SIZE);
        buffer.push_back(testData[i], DATA_BLOCK_SIZE);
    }
    ASSERT_EQ(buffer.size(), buffer.maxSize());
    buffer.pop_front(DATA_BLOCK_SIZE);

    int processedBytes = 0;
    int chIdx = 1;
    while (buffer.size() > 0)
    {
        QByteArray data = QByteArray::fromRawData(buffer.unfragmentedData(0, 16), 16);
        ASSERT_EQ(data.size(), 16);
        ASSERT_TRUE(isDataEq(data, testData[chIdx][0]));
        processedBytes += 16;
        if (processedBytes == DATA_BLOCK_SIZE) {
            processedBytes = 0;
            chIdx = (chIdx + 1) % DATA_BLOCKS;
        }
        buffer.pop_front(16);
    }

    // fill buffer again
    for (int i = 0; i < 16; ++i) {
        for (int i = 0; i < DATA_BLOCKS; ++i)
            buffer.push_back(testData[i], DATA_BLOCK_SIZE);
    }

    checkTestPattern(buffer);

    // move offset to the second half of the buffer
    for (int i = 0; i < 10; ++i) {
        for (int i = 0; i < DATA_BLOCKS; ++i) {
            buffer.pop_front(DATA_BLOCK_SIZE);
            buffer.push_back(testData[i], DATA_BLOCK_SIZE);
        }
    }

    checkTestPattern(buffer);
}
