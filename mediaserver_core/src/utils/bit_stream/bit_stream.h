#include <sstream>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <boost/dynamic_bitset.hpp>
#include <queue>
#include <atomic>
#include <functional>

template<typename Stream>
class BitStream
{
    typedef std::queue<boost::dynamic_bitset<>> WriteQueue;
public:
    struct Record
    {
        boost::dynamic_bitset<> record;
        std::function<void(Error)> callback;
    };
    
    enum class Error
    {
        NoError,
        ReadError,
        WriteError,
        ParseError,
        Eof
    };
    
    const size_t kPacketHeaderSize = 2;
    const size_t kFileHeaderSize = 16 * 8;
    
public:
    BitStream(Stream &stream) 
    : m_stream(stream),
      m_needStop(false)
    {
        startWriter();
    }
    
    ~BitStream() { stopWriter(); }
    
    boost::dynamic_bitset<> readFileHeader()
    {
        return readBits(kFileHeaderSize);
    }
    
    Error read() { return Error::ParseError; }
    
    template<typename PacketHead, typename ...PacketHandlers,
             typename ParseHead, typename ...ParseHandlers
            >
    Error read(PacketHead packetHandler, PacketHandlers ...packetHandlers,
               ParseHead parseHandler, ParseHandlers ...parseHandlers)
    {
        Error ret;
        std::lock_guard<std::mutex> lk(m_mutex);
        
        if (eof())
            ret = Error::Eof;
        else if (packetHandler(m_packetHeader))
        {
            getNextHeader();
            ret = parseHandler(this);
        }
        else
            ret = read(packetHandlers..., parseHandlers...);
            
        return ret;    
    }
    
    Error writeAsync(const boost::dynamic_bitset<> &record)
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_queue.push(record);
        }
        m_cond.notify_all();
    }
    
    boost::dynamic_bitset<> readBits(size_t n)
    {
        return boost::dynamic_bitset<>(n);
    }
    
private:
    void getNextHeader()
    {
        m_packetHeader = readBits(kPacketHeaderSize);
    }
    
    bool eof() const { return m_stream.eof(); }
    
    void startWriter()
    {
        m_thread = std::thread(
            [this]
            {
                while (!m_needStop)
                {
                    std::unique_lock<std::mutex> lk(m_mutex);
                    m_cond.wait(lk, [this] {return !m_queue.empty();});
                    
                    if (m_needStop)
                        return;
                }
            });
    }
    
    void stopWriter()
    {
        m_needStop = true;
        m_cond.notify_all();
        
        if (m_thread.joinable())
            m_thread.join();
    }
    
    void writeBits(const boost::dynamic_bitset<> &record)
    {
    }
    
private:
    Stream &m_stream;
    boost::dynamic_bitset<> m_packetHeader;
    
    mutable std::mutex m_mutex;
    std::thread m_thread;
    std::condition_variable m_cond;
    std::atomic<bool> m_needStop;
    
    WriteQueue m_queue;
    Error m_lastError;
};
