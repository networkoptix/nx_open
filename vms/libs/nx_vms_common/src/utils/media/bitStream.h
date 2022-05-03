// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <limits.h>
#include <stdexcept>

#include <nx/utils/log/assert.h>

#ifdef Q_OS_WIN
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

const static uint32_t INT_BIT = CHAR_BIT * sizeof(uint32_t);

class NX_VMS_COMMON_API BitStreamException: public std::exception
{
public:
    //BitStreamException(const char* str): std::exception(str) {}
    //BitStreamException(const std::string& str): std::exception(str.c_str()) {}
    BitStreamException(): std::exception() {}
    ~BitStreamException() throw() {}
    BitStreamException(const std::string& str): message(QLatin1String(str.c_str())) {}
    BitStreamException(const QString& str): message(str) {}
    BitStreamException(const char* str): message(QLatin1String(str)) {}

    virtual const char* what() const throw()
    {
        return message.toLatin1();
    }
private:
    QString message;
};

#define THROW_BITSTREAM_ERR throw BitStreamException()
#define THROW_BITSTREAM_ERR2(x) throw BitStreamException(x)

class BitStream
{
public:
    uint8_t* getBuffer() const { return (uint8_t*) m_initBuffer; }
    uint32_t getBitsLeft() const { return m_totalBits; }
protected:
    void setBuffer(const uint8_t* buffer, const uint8_t* end);
    uint32_t m_totalBits;
    uint32_t* m_buffer;
    uint32_t* m_initBuffer;
};

class NX_VMS_COMMON_API BitStreamReader: public BitStream
{
public:
    BitStreamReader() = default;
    BitStreamReader(const uint8_t* buffer, int size);
    BitStreamReader(const uint8_t* buffer, const uint8_t* end);

    void setBuffer(const uint8_t* buffer, const uint8_t* end);
    void setBuffer(const uint8_t* buffer, int size);
    uint32_t getBits(uint32_t num);
    uint32_t showBits(uint32_t num);
    uint32_t getBit();
    void skipBits(uint32_t num);
    void skipBytes(uint32_t num);
    void skipBit();
    void readData(uint8_t* data, int size);
    int bitsLeft() const { return m_totalBits; }
    int getGolomb();
    int getSignedGolomb();

    uint32_t getBitsCount() const
    {
        return (uint32_t)(m_buffer - m_initBuffer) * INT_BIT + INT_BIT - m_bitLeft;
    }

private:
    uint32_t getCurVal(uint32_t* buff);

private:
    uint32_t m_curVal;
    uint32_t m_bitLeft;
};

class NX_VMS_COMMON_API BitStreamWriter: public BitStream
{
public:
    BitStreamWriter() = default;
    BitStreamWriter(uint8_t* buffer, int size);
    BitStreamWriter(uint8_t* buffer, uint8_t* end);
    void setBuffer(uint8_t* buffer, uint8_t* end);
    void setBuffer(uint8_t* buffer, int size);
    void skipBits(uint32_t cnt);
    void putBits(uint32_t num, uint32_t value);
    void putBytes(const uint8_t* data, uint32_t size);
    void putBit(uint32_t value);
    void flushBits(bool finishLastByte = false);
    uint32_t getBitsCount();
    int getBytesCount();

private:
    uint32_t m_curVal;
    uint32_t m_bitsWritten;
};

void updateBits(const uint8_t* buffer, int bitOffset, int bitLen, int value);

// move len bits from oldBitOffset position to newBitOffset
void moveBits(uint8_t* buffer, int oldBitOffset, int newBitOffset, int len);
