#ifndef __BITSTREAM_H
#define __BITSTREAM_H

#include <assert.h>
#include <limits.h>
#include <stdexcept>

const static unsigned INT_BIT = CHAR_BIT * sizeof(unsigned);

class BitStreamException: public std::exception {
public:
	//BitStreamException(const char* str): std::exception(str) {}
	//BitStreamException(const std::string& str): std::exception(str.c_str()) {}
	BitStreamException(): std::exception() {}
	~BitStreamException() throw() {}
	BitStreamException(const std::string& str): message(QString::fromAscii(str.c_str())) {}
	BitStreamException(const QString& str): message(QString::fromAscii(str.toAscii())) {}
	BitStreamException(const char* str): message(QString::fromAscii(str)) {}

    virtual const char* what() const throw()
    {
        return message.toAscii();
    }
private:
    QString message;
};

#define THROW_BITSTREAM_ERR throw BitStreamException()
#define THROW_BITSTREAM_ERR2(x) throw BitStreamException(x)

class BitStream {
public:
	inline quint8* getBuffer() const {return (quint8*) m_initBuffer;}
	inline unsigned getBitsLeft() const {return m_totalBits;}
protected:
	inline void setBuffer(quint8* buffer, quint8* end)
	{
		if (buffer >= end)
			THROW_BITSTREAM_ERR;
		m_totalBits = (unsigned)(end - buffer) * 8;
		if (m_maskInitialized == 0) {
			for (unsigned i= 0; i < INT_BIT; i++)
				m_masks[i] = (1 << i) - 1;
			m_masks[INT_BIT] = UINT_MAX;
			m_maskInitialized = 1;
		}
		m_initBuffer = m_buffer = (unsigned*) buffer;
	}
	unsigned m_totalBits;
	unsigned* m_buffer;
	unsigned* m_initBuffer;
	static int m_maskInitialized;
	static unsigned m_masks[INT_BIT + 1];
};

class BitStreamReader: public BitStream {
private:
	inline unsigned getCurVal(unsigned* buff)
	{
		quint8* tmpBuf = (quint8*) buff;
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
public:
	inline void setBuffer(quint8* buffer, quint8* end) {
		BitStream::setBuffer(buffer, end);
		m_curVal = getCurVal(m_buffer);
		m_bitLeft = INT_BIT;
	}
	inline unsigned getBits(unsigned num)
	{
		if (num > INT_BIT)
			THROW_BITSTREAM_ERR;
		if (m_totalBits < num)
			THROW_BITSTREAM_ERR;
		unsigned prevVal = 0;
		if (num <= m_bitLeft)
			m_bitLeft -= num;
		else {
			prevVal = (m_curVal &  m_masks[m_bitLeft]) << (num - m_bitLeft);
			m_buffer++;
			m_curVal = getCurVal(m_buffer);
			m_bitLeft = INT_BIT - num + m_bitLeft;
		}
		m_totalBits -= num;
		return prevVal + (m_curVal >> m_bitLeft) & m_masks[num];
	}
	inline unsigned showBits(unsigned num)
	{
		assert(num <= INT_BIT);
		if (m_totalBits < num)
			THROW_BITSTREAM_ERR;
		unsigned prevVal = 0;
		unsigned bitLeft = m_bitLeft;
		unsigned curVal = m_curVal;
		if (num <= bitLeft)
			bitLeft -= num;
		else {
			prevVal = (curVal &  m_masks[bitLeft]) << (num - bitLeft);
			//curVal = ntohl(m_buffer[1]);
			curVal = getCurVal(m_buffer+1);
			bitLeft = INT_BIT - num + bitLeft;
		}
		return prevVal + (curVal >> bitLeft) & m_masks[num];
	}
	inline unsigned getBit() {
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
	inline void skipBits(unsigned num) {
		if (m_totalBits < num)
			THROW_BITSTREAM_ERR;
		assert(num <= INT_BIT);
		if (num <= m_bitLeft)
			m_bitLeft -= num;
		else {
			m_buffer++;
			m_curVal = getCurVal(m_buffer);
			m_bitLeft = INT_BIT - num + m_bitLeft;
		}
		m_totalBits -= num;
	}
	inline void skipBit() {
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
	inline unsigned getBitsCount() const  {return (unsigned)(m_buffer - m_initBuffer) * INT_BIT + INT_BIT - m_bitLeft;}
private:
	unsigned m_curVal;
	unsigned m_bitLeft;
};


class BitStreamWriter: public BitStream {
public:
	inline void setBuffer(quint8* buffer, quint8* end) {
		BitStream::setBuffer(buffer, end);
		m_curVal = 0;
		m_bitWrited = 0;
	}
	inline void skipBits(unsigned cnt)
	{
		assert(m_bitWrited % INT_BIT == 0);
		BitStreamReader reader;
		reader.setBuffer((quint8*)m_buffer, (quint8*) (m_buffer + 1));
		putBits(cnt, reader.getBits(cnt));
	}
	inline void putBits(unsigned num, unsigned value) {
		if (m_totalBits < num)
			THROW_BITSTREAM_ERR;
		value &= m_masks[num];
		if (m_bitWrited + num < INT_BIT) {
			m_bitWrited += num;
			m_curVal <<= num;
			m_curVal += value;
		}
		else {
			m_curVal <<= (INT_BIT - m_bitWrited);
			m_bitWrited = m_bitWrited + num - INT_BIT;
			m_curVal += value >> m_bitWrited;
			*m_buffer++ = htonl(m_curVal);
			m_curVal = value & m_masks[m_bitWrited];
		}
		m_totalBits -= num;
	}
	inline void putBit(unsigned value) {
		if (m_totalBits < 1)
			THROW_BITSTREAM_ERR;
		value &= m_masks[1];
		if (m_bitWrited + 1 < INT_BIT) {
			m_bitWrited ++;
			m_curVal <<= 1;
			m_curVal += value;
		}
		else {
			m_curVal <<= (INT_BIT - m_bitWrited);
			m_bitWrited = m_bitWrited + 1 - INT_BIT;
			m_curVal += value >> m_bitWrited;
			*m_buffer++ = htonl(m_curVal);
			m_curVal = value & m_masks[m_bitWrited];
		}
		m_totalBits --;
	}
	inline void flushBits() {
		m_curVal <<= INT_BIT - m_bitWrited;
		unsigned prevVal = ntohl(*m_buffer);
		prevVal &= m_masks[INT_BIT - m_bitWrited];
		prevVal |= m_curVal;
		*m_buffer = htonl(prevVal);
	}
	inline unsigned getBitsCount() {
		return (unsigned) (m_buffer - m_initBuffer) * INT_BIT + m_bitWrited;
	}
private:
	unsigned m_curVal;
	unsigned m_bitWrited;
};

void updateBits(const BitStreamReader& bitReader, int bitOffset, int bitLen, int value);
void updateBits(const quint8* buffer, int bitOffset, int bitLen, int value);

// move len bits from oldBitOffset position to newBitOffset
void moveBits(quint8* buffer, int oldBitOffset, int newBitOffset, int len);


#endif
