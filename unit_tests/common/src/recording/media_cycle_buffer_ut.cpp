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

void checkTestPattern(QnMediaCycleBuffer& buffer)
{
    QByteArray testString("TEST_DATA");
    for (int i = 1; i < buffer.size() - testString.size() - 1; ++i)
    {
        buffer.update(0, "B", 1);
        buffer.update(buffer.size()-1, "E", 1);
        buffer.update(i, testString.data(), testString.size());
        QByteArray resultData = buffer.data(i, testString.size());
        ASSERT_EQ(testString, resultData);
        ASSERT_EQ(buffer.data(0, 1), QByteArray("B", 1));
        ASSERT_EQ(buffer.data(buffer.size()-1, 1), QByteArray("E", 1));
    }
}

TEST( QnMediaCycleBuffer, main )
{
    static const int DATA_BLOCK_SIZE = 32;
    static const int DATA_BLOCKS = 5;

    QnMediaCycleBuffer buffer(DATA_BLOCK_SIZE * DATA_BLOCKS * 16, 64);

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
        QByteArray data = buffer.data(0, 16);
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
