////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PLUGINUSAGEWATCHER_H
#define PLUGINUSAGEWATCHER_H

#include <QList>

#include <decoders/video/abstractdecoder.h>
#include <common/videodecoderplugintypes.h>


//!Monitors usage of video decoder plugin between all processes, using it
/*!
    Provides list of streams, decoded by plugin with stream parameters:\n
    - picture resolution
    - codec profile
    - current fps
*/
class PluginUsageWatcher
{
public:
    /*!
        \param uniquePluginID ID used to distinguish different plugins on system. It is recommended that UID is found here
    */
    PluginUsageWatcher( const QByteArray& uniquePluginID );

    //!Returns list of current decode sessions, using this plugin
    QList<DecoderStreamDescription> currentSessions() const;
    //!Add decoder \a decoder to the list of current sessions
    /*!
        Before \a decoder destruction one MUST call method \a decoderIsAboutToBeDestroyed
    */
    void decoderCreated( QnAbstractVideoDecoder* const decoder );
    //!Removes \a decoder from current session list
    void decoderIsAboutToBeDestroyed( QnAbstractVideoDecoder* const decoder );
};

#endif  //PLUGINUSAGEWATCHER_H
