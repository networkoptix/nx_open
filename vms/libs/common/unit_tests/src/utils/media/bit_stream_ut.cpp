
#include <gtest/gtest.h>

#include <utils/media/bitStream.h>


TEST(BitStream, bufferOverflow)
{
    {
        const int size = 4;
        uint8_t data[size] = {0xcd, 0xcd, 0xcd, 0xcd};
        BitStreamWriter writer(data, data + 2);

        writer.putBits(4, 1);
        writer.putBits(4, 1);
        writer.putBits(8, 1);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00010001);
        ASSERT_EQ(data[1], 0b00000001);
        ASSERT_EQ(data[2], 0xcd);
        ASSERT_EQ(data[3], 0xcd);
    }

    {
        const int size = 5;
        uint8_t data[size] = {0xcd, 0xcd, 0xcd, 0xcd, 0xcd};
        BitStreamWriter writer(data, data + 4);

        writer.putBits(16, 1);
        writer.putBits(16, 1);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00000000);
        ASSERT_EQ(data[1], 0b00000001);
        ASSERT_EQ(data[2], 0b00000000);
        ASSERT_EQ(data[3], 0b00000001);
        ASSERT_EQ(data[4], 0xcd);
    }
}

TEST(BitStream, readBytes)
{
    {
        uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        BitStreamReader reader(data, data + sizeof(data));

        uint8_t readData[4] = {0};
        reader.readData(readData, sizeof(readData));
        ASSERT_EQ(readData[0], 0x00);
        ASSERT_EQ(readData[1], 0x01);
        ASSERT_EQ(readData[2], 0x02);
        ASSERT_EQ(readData[3], 0x03);
        ASSERT_EQ(reader.getBits(4), 0x00);
        ASSERT_EQ(reader.getBits(4), 0x04);
    }

    {
        uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        BitStreamReader reader(data, data + sizeof(data));

        reader.getBits(8);
        uint8_t readData[2] = {0};
        reader.readData(readData, sizeof(readData));
        ASSERT_EQ(readData[0], 0x01);
        ASSERT_EQ(readData[1], 0x02);
        ASSERT_EQ(reader.getBits(4), 0x00);
        ASSERT_EQ(reader.getBits(4), 0x03);
    }
    {
        uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        BitStreamReader reader(data, data + sizeof(data));

        reader.getBits(16);
        uint8_t readData[2] = {0};
        reader.readData(readData, sizeof(readData));
        ASSERT_EQ(readData[0], 0x02);
        ASSERT_EQ(readData[1], 0x03);
        ASSERT_EQ(reader.getBits(4), 0x00);
        ASSERT_EQ(reader.getBits(4), 0x04);
    }
    {
        uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
        BitStreamReader reader(data, data + sizeof(data));

        reader.getBits(16);
        uint8_t readData[4] = {0};
        reader.readData(readData, sizeof(readData));
        ASSERT_EQ(readData[0], 0x02);
        ASSERT_EQ(readData[1], 0x03);
        ASSERT_EQ(readData[2], 0x04);
        ASSERT_EQ(readData[3], 0x05);
        ASSERT_EQ(reader.getBits(4), 0x00);
        ASSERT_EQ(reader.getBits(4), 0x06);
    }
}

TEST(BitStream, write)
{
    {
        const int size = 2;
        uint8_t data[size];
        BitStreamWriter writer(data, data + size);

        writer.putBits(4, 1);
        writer.putBits(4, 1);
        writer.putBits(8, 1);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00010001);
        ASSERT_EQ(data[1], 0b00000001);
    }
    {
        const int size = 4;
        uint8_t data[size];
        BitStreamWriter writer(data, data + size);

        writer.putBits(16, 1);
        writer.putBits(16, 1);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00000000);
        ASSERT_EQ(data[1], 0b00000001);
        ASSERT_EQ(data[2], 0b00000000);
        ASSERT_EQ(data[3], 0b00000001);
    }
    {
        const int size = 3;
        uint8_t data[size];
        BitStreamWriter writer(data, data + size);

        writer.putBits(8, 1);
        writer.putBits(16, 1);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00000001);
        ASSERT_EQ(data[1], 0b00000000);
        ASSERT_EQ(data[2], 0b00000001);
    }
    {
        const int size = 6;
        uint8_t data[size];
        BitStreamWriter writer(data, data + size);

        writer.putBits(16, 1);
        writer.putBits(32, 0b01010101);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00000000);
        ASSERT_EQ(data[1], 0b00000001);
        ASSERT_EQ(data[2], 0b00000000);
        ASSERT_EQ(data[3], 0b00000000);
        ASSERT_EQ(data[4], 0b00000000);
        ASSERT_EQ(data[5], 0b01010101);
    }
    {
        const int size = 6;
        uint8_t data[size] = {0};
        BitStreamWriter writer(data, data + size);

        writer.putBits(16, 1);
        writer.putBits(30, 0b11);
        writer.flushBits();

        ASSERT_EQ(data[0], 0b00000000);
        ASSERT_EQ(data[1], 0b00000001);
        ASSERT_EQ(data[2], 0b00000000);
        ASSERT_EQ(data[3], 0b00000000);
        ASSERT_EQ(data[4], 0b00000000);
        ASSERT_EQ(data[5], 0b00001100);
    }
    {
        const int size = 6;
        uint8_t data[size] = {0};
        BitStreamWriter writer(data, data + size);

        writer.putBits(7, 0b1101);
        writer.putBits(1, 1);
        writer.putBits(3, 0b1);
        writer.putBits(1, 0);
        writer.flushBits(true);

        ASSERT_EQ(data[0], 0b00011011);
        ASSERT_EQ(data[1], 0b00100000);
    }
}



