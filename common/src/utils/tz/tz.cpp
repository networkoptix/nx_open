/**********************************************************
* 21 aug 2014
* a.kolesnikov
***********************************************************/

#include "tz.h"

#include <sys/stat.h>
#include <sys/timeb.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>

#include <QtCore/QByteArray>

#include "tzdb_data.h"


namespace nx_tz
{
    class TZIndex
    {
    public:
        TZIndex();

        ISOTimezoneData getTzOffsets(const QByteArray& tzName) const;

        static TZIndex* instance();

    private:
        std::map<QByteArray, ISOTimezoneData> m_tzNameToTzData;
    };

    TZIndex::TZIndex()
    {
        for( size_t i = 0; i < sizeof(timezones) / sizeof(*timezones); ++i )
        {
            m_tzNameToTzData.emplace(
                QByteArray::fromRawData(timezones[i].tz, (int)strlen(timezones[i].tz)),
                timezones[i]);
        }
    }

    ISOTimezoneData TZIndex::getTzOffsets(const QByteArray& tzName) const
    {
        ISOTimezoneData result;
        memset(&result, 0, sizeof(result));
        auto it = m_tzNameToTzData.find( tzName );
        if( it == m_tzNameToTzData.end() )
            return result;
        return it->second;
    }

    static std::unique_ptr<TZIndex> TZIndex_instance;
    static std::once_flag TZIndex_onceFlag;

    TZIndex* TZIndex::instance()
    {
        std::call_once(
            TZIndex_onceFlag,
            [](){ TZIndex_instance.reset( new TZIndex() ); } );

        return TZIndex_instance.get();
    }




    static const int MIN_PER_HOUR = 60;
    static const int SEC_PER_MIN = 60;

    int getLocalTimeZoneOffset()
    {
#ifdef _WIN32
        int daylight;
        _get_daylight(&daylight);
        long timezone = 0;
        _get_timezone(&timezone);
        return -(timezone / SEC_PER_MIN) + (daylight ? MIN_PER_HOUR : 0);
#else //if defined(__linux__)
        //cannot rely on ftime on linux

        ISOTimezoneData tzData;
        std::ifstream tzNameFile( "/etc/timezone" );
        if( tzNameFile.is_open() )
        {
            char buf[256];
            memset( buf, 0, sizeof(buf) );
            tzNameFile.read(buf, sizeof(buf)-1);
            if( !tzNameFile.eof() )
                return -1;
            tzData = TZIndex::instance()->getTzOffsets(QByteArray::fromRawData(buf, tzNameFile.gcount()).trimmed());
            tzNameFile.close();
        }
        else
        {
            //trying /etc/localtime
            for( int i = 0; i < 3; ++i )
            {
                struct stat st;
                memset( &st, 0, sizeof(st) );
                if( lstat( "/etc/localtime", &st ) != 0 )
                    break;
                if( !S_ISLNK(st.st_mode) )
                    break;

                //reading link /etc/localtime
                auto free_del = [](char* ptr){::free(ptr);};
                std::unique_ptr<char, decltype(free_del)> linkTarget( (char*)malloc(st.st_size+1), free_del );
                if( !linkTarget.get() )
                    break;
                int bytesRead = readlink( "/etc/localtime", linkTarget.get(), st.st_size+1 );
                if( bytesRead == -1 )
                    break;
                if( bytesRead > st.st_size )
                    continue;   //link increased in size between stat and readlink, trying once again
                linkTarget.get()[bytesRead] = '\0';
                //getting timezone from link
                if( strncmp( linkTarget.get(), "/usr/share/zoneinfo/", sizeof("/usr/share/zoneinfo/")-1 ) != 0 )
                    break;  //timezone link point to non-standard path  //TODO #ak better support that anyway
                const char* tzName = linkTarget.get() + sizeof("/usr/share/zoneinfo/")-1;
                tzData = TZIndex::instance()->getTzOffsets( QByteArray::fromRawData(tzName, strlen(tzName)) );
                break;
            }
        }
        if( tzData.tz == nullptr )
            return -1;
        const time_t curUtcTime = time(NULL);
        struct tm locTime;
        memset(&locTime, 0, sizeof(locTime));
        localtime_r(&curUtcTime, &locTime);
        return locTime.tm_isdst ? tzData.utcDSTOffset : tzData.utcOffset;
#endif
    }
}
