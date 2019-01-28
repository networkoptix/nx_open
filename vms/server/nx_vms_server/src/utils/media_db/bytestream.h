#pragma once

class ByteStreamReader
{
public:
    ByteStreamReader(const QByteArray& data):
        src(data.data()),
        end(data.data() + data.size())
    {
    }

    quint64 readUint64()
    {
        quint64* src64 = (quint64*) src;
        src += sizeof(quint64);
        return qFromLittleEndian(*src64);
    }

    int readRawData(char* dst, int len)
    {
        if (src > end - len)
            return -1;
        memcpy(dst, src, len);
        src += len;
        return len;
    }


    bool hasBuffer(int rest) const
    {
        return src <= end - rest;
    }

private:
    const char* src;
    const char* end;
};

class ByteStreamWriter
{
public:
    ByteStreamWriter(int bufferSize)
    {
        m_data.resize(bufferSize);
        m_dst = m_data.data();
        m_end = m_dst + m_data.size();
    }

    void writeUint64(quint64 value)
    {
        ensureFreeSize(sizeof(quint64));

        quint64* dst64 = (quint64*)m_dst;
        m_dst += sizeof(quint64);
        *dst64 = qToLittleEndian(value);
    }

    void writeRawData(const char* src, int len)
    {
        ensureFreeSize(len);

        memcpy(m_dst, src, len);
        m_dst += len;
    }

    void ensureFreeSize(int size)
    {
        if (m_dst > m_end - size)
        {
            auto offset = m_dst - m_data.data();
            m_data.resize(m_data.size() * 2);
            m_dst = m_data.data() + offset;
            m_end = m_data.data() + m_data.size();
        }
    }

    const QByteArray& data() const
    {
        return m_data;
    }

    void flush()
    {
        m_data.truncate(m_dst - m_data.data());
    }

private:
    QByteArray m_data;
    char* m_dst = nullptr;
    const char* m_end = nullptr;
};
