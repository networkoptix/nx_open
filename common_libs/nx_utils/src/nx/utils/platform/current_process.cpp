#include "current_process.h"

#if defined( Q_OS_LINUX ) || defined( Q_OS_MAC )
    #include <pwd.h>
    #include <sys/prctl.h>
    #include <sys/types.h>
    #include <unistd.h>

    static bool changeUser( struct passwd* pwd )
    {
        if( !pwd ||
            setregid( pwd->pw_gid, pwd->pw_gid ) < 0 ||
            setreuid( pwd->pw_uid, pwd->pw_uid ) < 0)
        {
            // TODO: qWarning or NX_LOG the real error?
            return false;
        }

        // Allow core dump creation.
        prctl(PR_SET_DUMPABLE, 1);
        return true;
    }
#endif

namespace nx {
namespace utils {

const bool CurrentProcess::changeUser( const QString& userName )
{
    #if defined( Q_OS_LINUX ) || defined( Q_OS_MAC )
        return ::changeUser( getpwnam( userName.toStdString().c_str() ) );
    #else
        return false;
    #endif
}

const bool CurrentProcess::changeUser( const uint userId )
{
    #if defined( Q_OS_LINUX ) || defined( Q_OS_MAC )
        return ::changeUser( getpwuid( static_cast< uid_t >( userId ) ) );
    #else
        return false;
    #endif
}

} // namespace utils
} // namespace nx
