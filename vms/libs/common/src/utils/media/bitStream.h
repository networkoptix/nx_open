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

class BitStreamException: public std::exception {
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
    inline uint8_t* getBuffer() const {return (uint8_t*) m_initBuffer;}
    inline uint32_t getBitsLeft() const {return m_totalBits;}
protected:
    void setBuffer(const uint8_t* buffer, const uint8_t* end);
    uint32_t m_totalBits;
    uint32_t* m_buffer;
    uint32_t* m_initBuffer;
};

class BitStreamReader: public BitStream
{
public:
    BitStreamReader() = default;
    BitStreamReader(const uint8_t* buffer, int size);
    BitStreamReader(const uint8_t* buffer, const uint8_t* end);

    void setBuffer(const uint8_t* buffer, const uint8_t* end);
    uint32_t getBits(uint32_t num);
    uint32_t showBits(uint32_t num);
    uint32_t getBit();
    void skipBits(uint32_t num);
    void skipBytes(uint32_t num);
    void skipBit();
    inline uint32_t getBitsCount() const  {return (uint32_t)(m_buffer - m_initBuffer) * INT_BIT + INT_BIT - m_bitLeft;}
    inline int bitsLeft() const { return m_totalBits; }

private:
    uint32_t getCurVal(uint32_t* buff);

private:
    uint32_t m_curVal;
    uint32_t m_bitLeft;
};

class BitStreamWriter: public BitStream
{
public:
    BitStreamWriter() = default;
    BitStreamWriter(uint8_t* buffer, uint8_t* end);
    void setBuffer(uint8_t* buffer, uint8_t* end);
    void setBuffer(uint8_t* buffer, int size);
    void skipBits(uint32_t cnt);
    void putBits(uint32_t num, uint32_t value);
    void putBytes(uint8_t* data, uint32_t size);
    void putBit(uint32_t value);
    void flushBits(bool finishLastByte = false);
    uint32_t getBitsCount();
    int getBytesCount();

private:
    uint32_t m_curVal;
    uint32_t m_bitsWritten;
};

void updateBits(const BitStreamReader& bitReader, int bitOffset, int bitLen, int value);
void updateBits(const uint8_t* buffer, int bitOffset, int bitLen, int value);

// move len bits from oldBitOffset position to newBitOffset
void moveBits(uint8_t* buffer, int oldBitOffset, int newBitOffset, int len);
