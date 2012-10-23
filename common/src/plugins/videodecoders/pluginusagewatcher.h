////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PLUGINUSAGEWATCHER_H
#define PLUGINUSAGEWATCHER_H

#include <set>

#include <QMutex>

#include "videodecoderplugintypes.h"
#include "../../decoders/video/abstractdecoder.h"


//!Monitors usage of video decoder plugin between all processes, using it
/*!
    Provides list of streams, decoded by plugin with stream parameters:\n
    - picture resolution
    - codec profile
    - current fps

    \note Methods of this class are thread-safe
*/
class PluginUsageWatcher
{
public:
    /*!
        \param uniquePluginID ID used to distinguish different plugins on system. It is recommended that UID is found here
    */
    PluginUsageWatcher( const QByteArray& uniquePluginID );

    //!Returns list of current decode sessions, using this plugin
    /*!
        \note Returned data is actual for the moment of calling this method
    */
    std::set<stree::AbstractResourceReader*> currentSessions() const;
    //!Returns number of current decode sessions, using this plugin
    size_t currentSessionCount() const;
    //!Returns parameters, describing total plugin usage (total pixels per second, total fps, etc...)
    /*!
        Method returns following resources:\n
            - framePictureSize
            - fps
            - pixelsPerSecond
            - videoMemoryUsage
        Every value is a sum of values of all active sessions
        \note Returned data is actual for the moment of calling this method
    */
    stree::ResourceContainer currentTotalUsage() const;
    //!Add decoder \a decoder to the list of current sessions
    /*!
        Before \a decoder destruction one MUST call method \a decoderIsAboutToBeDestroyed
    */
    void decoderCreated( stree::AbstractResourceReader* const decoder );
    //!Removes \a decoder from current session list
    void decoderIsAboutToBeDestroyed( stree::AbstractResourceReader* const decoder );

private:
    mutable QMutex m_mutex;
    QByteArray m_uniquePluginID;
    std::set<stree::AbstractResourceReader*> m_currentSessions;
};

#endif  //PLUGINUSAGEWATCHER_H
