////////////////////////////////////////////////////////////
// 22 aug 2012    Andrey kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTCLIENTPLUGIN_H
#define ABSTRACTCLIENTPLUGIN_H

#include <QObject>


//!Base class for all dynamically-linked plugins to client
class QnAbstractClientPlugin
:
    public QObject
{
    Q_OBJECT

public:
    //!Returns minimum client version required for plugin
    /*!
        Returned version is represented as follows:
        byte 3 - major version
    */
    virtual quint32 minSupportedVersion() const = 0;
};

#endif  //ABSTRACTCLIENTPLUGIN_H
