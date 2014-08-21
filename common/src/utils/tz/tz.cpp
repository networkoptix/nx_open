/**********************************************************
* 21 aug 2014
* a.kolesnikov
***********************************************************/

#include "tz.h"

#include <sys/timeb.h>

#include <atomic>
#include <fstream>
#include <mutex>

#include <QtCore/QByteArray>

#include "tzdb_data.h"


namespace nx_tz
{
    class TZIndex
    {
    public:
        TZIndex()
        {
            for( size_t i = 0; i < sizeof(timezones) / sizeof(*timezones); ++i )
            {
                m_tzNameToTzData.emplace(
                    QByteArray::fromRawData(timezones[i].tz, (int)strlen(timezones[i].tz)),
                    timezones[i]);
            }
        }

        ISOTimezoneData getTzOffsets(const QByteArray& tzName) const
        {
            ISOTimezoneData result;
            memset(&result, 0, sizeof(result));
            auto it = m_tzNameToTzData.find( tzName );
            if( it == m_tzNameToTzData.end() )
                return result;
            return it->second;
        }


        static std::atomic<TZIndex*> tzIndexInstance;
        static std::mutex tzIndexInstanciationMutex;

        static TZIndex* instance()
        {
            TZIndex* tzIndex = tzIndexInstance.load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if( tzIndex == nullptr )
            {
                std::lock_guard<std::mutex> lock(tzIndexInstanciationMutex);
                tzIndex = tzIndexInstance.load(std::memory_order_relaxed);
                if( tzIndex == nullptr )
                {
                    //if required, building tz index
                    tzIndex = new TZIndex();
                    std::atomic_thread_fence(std::memory_order_release);
                    tzIndexInstance.store(tzIndex, std::memory_order_relaxed);
                }
            }
            return tzIndex;
        }

    private:
        std::map<QByteArray, ISOTimezoneData> m_tzNameToTzData;
    };

    std::atomic<TZIndex*> TZIndex::tzIndexInstance;
    std::mutex TZIndex::tzIndexInstanciationMutex;



    int getLocalTimeZoneOffset()
    {
#ifdef _WIN32
        struct timeb tp;
        memset(&tp, 0, sizeof(tp));
        ftime(&tp);
        return -tp.timezone;
#else //if defined(__linux__)
        //cannot rely on ftime on linux

        std::ifstream tzNameFile( "/etc/timezone" );
        char buf[256];
        memset( buf, 0, sizeof(buf) );
        tzNameFile.read(buf, sizeof(buf)-1);
        if( !tzNameFile.eof() )
            return -1;
        ISOTimezoneData tzData = TZIndex::instance()->getTzOffsets(QByteArray::fromRawData(buf, tzNameFile.gcount()).trimmed());
        if( tzData.tz == nullptr )
            return -1;
        return tzData.utcOffset;
#endif
    }
}
