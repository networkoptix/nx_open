#include "bitStream.h"


constexpr uint32_t kMask[] = {
    0000000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff};

void updateBits(const BitStreamReader& bitReader, int bitOffset, int bitLen, int value)
{
    updateBits(bitReader.getBuffer(), bitOffset, bitLen, value);
}

void updateBits(const uint8_t* buffer, int bitOffset, int bitLen, int value)
{
    uint8_t* ptr = (uint8_t*) buffer + bitOffset/8;
    BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    uint8_t* ptr_end = (uint8_t*) buffer + (bitOffset + bitLen)/8;
    int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0) {
        int prefix = *ptr >> (8-byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8) {
        int postfix = *ptr_end & (( 1 << endBitsPostfix) - 1);
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

void moveBits(uint8_t* buffer, int oldBitOffset, int newBitOffset, int len)
{
    int increaseInBytes = qMax(0, (newBitOffset - oldBitOffset)/8+1);

    uint8_t* src = (uint8_t*) buffer + (oldBitOffset >> 3);
    BitStreamReader reader;
    reader.setBuffer(src, src + len/8 + 1);
    uint8_t* dst = (uint8_t*) buffer + (newBitOffset >> 3);
    BitStreamWriter writer;
    writer.setBuffer(dst, dst + len/8 + 1 + increaseInBytes);
    writer.skipBits(newBitOffset % 8);
    if (oldBitOffset % 8) 
    {
        reader.skipBits(oldBitOffset % 8);
        int c = 8 - (oldBitOffset % 8);
        writer.putBits(c, reader.getBits(c));
        len -= c;
        src++;
    }
    for (; len >= 8 && ((unsigned long) src % sizeof(uint32_t)) != 0; len -= 8) {
        writer.putBits(8, *src);
        src++;
    }

    for (; len >= (int)INT_BIT; len -= INT_BIT) {
        writer.putBits(INT_BIT, ntohl(*((uint32_t*)src)));
        src += sizeof(uint32_t);
    }
    reader.setBuffer(src, src + sizeof(uint32_t));
    writer.putBits(len, reader.getBits(len));
    writer.flushBits();
}

void BitStream::setBuffer(const uint8_t* buffer, const uint8_t* end)
{
    if (buffer >= end)
        THROW_BITSTREAM_ERR;
    m_totalBits = (uint32_t)(end - buffer) * 8;
    m_initBuffer = m_buffer = (uint32_t*) buffer;
}

BitStreamReader::BitStreamReader(const uint8_t* buffer, int size)
{
    setBuffer(buffer, buffer + size);
}

BitStreamReader::BitStreamReader(const uint8_t* buffer, const uint8_t* end)
{
    setBuffer(buffer, end);
}

uint32_t BitStreamReader::getCurVal(uint32_t* buff)
{
    uint8_t* tmpBuf = (uint8_t*) buff;
    if (m_totalBits >= 32)
        return ntohl(*buff);
    else if (m_totalBits >= 24)
        return (tmpBuf[0] << 24) + (tmpBuf[1] << 16) + (tmpBuf[2] << 8);
    else if (m_totalBits >= 16)
        return (tmpBuf[0] << 24) + (tmpBuf[1] << 16);
    else if (m_totalBits >= 8)
        return tmpBuf[0] << 24;
    else
        THROW_BITSTREAM_ERR;
}

void BitStreamReader::setBuffer(const uint8_t* buffer, const uint8_t* end)
{
    BitStream::setBuffer(buffer, end);
    m_curVal = getCurVal(m_buffer);
    m_bitLeft = INT_BIT;
}

uint32_t BitStreamReader::getBits(uint32_t num)
{
    if (num > INT_BIT)
        THROW_BITSTREAM_ERR;
    if (m_totalBits < num)
        THROW_BITSTREAM_ERR;
    uint32_t prevVal = 0;
    if (num <= m_bitLeft)
        m_bitLeft -= num;
    else {
        prevVal = (m_curVal &  kMask[m_bitLeft]) << (num - m_bitLeft);
        m_buffer++;
        m_curVal = getCurVal(m_buffer);
        m_bitLeft = INT_BIT - num + m_bitLeft;
    }
    m_totalBits -= num;
    return (prevVal + (m_curVal >> m_bitLeft)) & kMask[num];
}

uint32_t BitStreamReader::showBits(uint32_t num)
{
    NX_ASSERT(num <= INT_BIT);
    if (m_totalBits < num)
        THROW_BITSTREAM_ERR;
    uint32_t prevVal = 0;
    uint32_t bitLeft = m_bitLeft;
    uint32_t curVal = m_curVal;
    if (num <= bitLeft)
        bitLeft -= num;
    else {
        prevVal = (curVal &  kMask[bitLeft]) << (num - bitLeft);
        //curVal = ntohl(m_buffer[1]);
        curVal = getCurVal(m_buffer+1);
        bitLeft = INT_BIT - num + bitLeft;
    }
    return (prevVal + (curVal >> bitLeft)) & kMask[num];
}

uint32_t BitStreamReader::getBit()
{
    if (m_totalBits < 1)
        THROW_BITSTREAM_ERR;
    if (m_bitLeft > 0)
        m_bitLeft--;
    else {
        m_buffer++;
        m_curVal = getCurVal(m_buffer);
        m_bitLeft = INT_BIT - 1;
    }
    m_totalBits--;
    return (m_curVal >> m_bitLeft) & 1;
}

void BitStreamReader::skipBits(uint32_t num)
{
    if (m_totalBits < num)
        THROW_BITSTREAM_ERR;
    NX_ASSERT(num <= INT_BIT);
    if (num <= m_bitLeft)
        m_bitLeft -= num;
    else {
        m_buffer++;
        m_curVal = getCurVal(m_buffer);
        m_bitLeft = INT_BIT - num + m_bitLeft;
    }
    m_totalBits -= num;
}

void BitStreamReader::skipBytes(uint32_t num)
{
    if (m_totalBits < num * 8)
        THROW_BITSTREAM_ERR;
    while (m_bitLeft > 0 && num > 0)
    {
        skipBits(8);
        --num;
    }
    uint32_t worldsToSkip = num >> 2;
    m_buffer += worldsToSkip;
    m_totalBits -= worldsToSkip * 32;
    num &= 3;
    skipBits(num * 8);
}

void BitStreamReader::skipBit()
{
    if (m_totalBits < 1)
        THROW_BITSTREAM_ERR;
    if (m_bitLeft > 0)
        m_bitLeft--;
    else {
        m_buffer++;
        m_curVal = getCurVal(m_buffer);
        m_bitLeft = INT_BIT - 1;
    }
    m_totalBits--;
}

void BitStreamWriter::putBits(uint32_t num, uint32_t value)
{
    if (m_totalBits < num)
        THROW_BITSTREAM_ERR;
    value &= kMask[num];
    if (m_bitsWritten + num < INT_BIT) {
        m_bitsWritten += num;
        m_curVal <<= num;
        m_curVal += value;
    }
    else {
        m_curVal <<= (INT_BIT - m_bitsWritten);
        m_bitsWritten = m_bitsWritten + num - INT_BIT;
        m_curVal += value >> m_bitsWritten;
        *m_buffer++ = htonl(m_curVal);
        m_curVal = value & kMask[m_bitsWritten];
    }
    m_totalBits -= num;
}

BitStreamWriter::BitStreamWriter(uint8_t* buffer, uint8_t* end)
{
    setBuffer(buffer, end);
}

void BitStreamWriter::setBuffer(uint8_t* buffer, uint8_t* end)
{
    BitStream::setBuffer(buffer, end);
    m_curVal = 0;
    m_bitsWritten = 0;
}

void BitStreamWriter::setBuffer(uint8_t* buffer, int size)
{
    BitStream::setBuffer(buffer, buffer + size);
    m_curVal = 0;
    m_bitsWritten = 0;
}

void BitStreamWriter::skipBits(uint32_t cnt)
{
    NX_ASSERT(m_bitsWritten % INT_BIT == 0);
    BitStreamReader reader;
    reader.setBuffer((uint8_t*)m_buffer, (uint8_t*) (m_buffer + 1));
    putBits(cnt, reader.getBits(cnt));
}

void BitStreamWriter::putBytes(uint8_t* data, uint32_t size)
{
    if (m_totalBits < size)
        THROW_BITSTREAM_ERR;
    while (m_bitsWritten > 0 && size > 0)
    {
        putBits(8, *data++);
        --size;
    }

    int copySize = size & ~3; //< flor to 4
    if (copySize > 0)
    {
        memcpy(m_buffer, data, copySize);
        m_buffer += copySize / 4;
        data += copySize;
        size -= copySize;
        m_totalBits -= copySize * 8;
    }

    while (size > 0)
    {
        putBits(8, *data++);
        --size;
    }
}

void BitStreamWriter::putBit(uint32_t value)
{
    if (m_totalBits < 1)
        THROW_BITSTREAM_ERR;
    value &= kMask[1];
    if (m_bitsWritten + 1 < INT_BIT)
    {
        m_bitsWritten ++;
        m_curVal <<= 1;
        m_curVal += value;
    }
    else
    {
        m_curVal <<= (INT_BIT - m_bitsWritten);
        m_bitsWritten = m_bitsWritten + 1 - INT_BIT;
        m_curVal += value >> m_bitsWritten;
        *m_buffer++ = htonl(m_curVal);
        m_curVal = value & kMask[m_bitsWritten];
    }
    m_totalBits --;
}

void BitStreamWriter::flushBits(bool finishLastByte)
{
    if (finishLastByte)
    {
        int bitsLeft = 8 - (m_bitsWritten % 8);
        if (bitsLeft < 8)
            putBits(bitsLeft, 0);
    }

    m_curVal <<= INT_BIT - m_bitsWritten;
    uint32_t prevVal = ntohl(*m_buffer);
    prevVal &= kMask[INT_BIT - m_bitsWritten];
    prevVal |= m_curVal;
    *m_buffer = htonl(prevVal);
}

uint32_t BitStreamWriter::getBitsCount()
{
    return (uint32_t) (m_buffer - m_initBuffer) * INT_BIT + m_bitsWritten;
}

int BitStreamWriter::getBytesCount()
{
    return (getBitsCount() + 7) / 8;
}
