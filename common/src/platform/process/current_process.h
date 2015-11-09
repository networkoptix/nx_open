#ifndef PLATFORM_PROCESS_CURRENT_PROCESS_H
#define PLATFORM_PROCESS_CURRENT_PROCESS_H

#include <QString>

namespace nx {

class CurrentProcess
{
public:
    /** linux, osx: changes real and effective user and group IDs to \arg userName
     *  windows: always returns false ^_^ */
    static const bool changeUser( const QString& userName );

    /** linux, osx: changes real and effective user and group IDs to \arg userId
     *  windows: always returns false ^_^ */
    static const bool changeUser( const uint userId );

private:
    CurrentProcess(); // not constructable
};

} // namespace nx

#endif // PLATFORM_PROCESS_CURRENT_PROCESS_H
