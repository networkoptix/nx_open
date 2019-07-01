
#include <gtest/gtest.h>

#include <utils/media/bitStream.h>

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



