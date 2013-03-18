////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef APPLICATION_TASK_H
#define APPLICATION_TASK_H

#include <QString>


//!Task, sent by application, to start specified application version
class StartApplicationTask
{
public:
    //!Version in serialized format (e.g., 1.4.3)
    QString version;
    //!Command-line params to pass to application instance
    QString appArgs;

    StartApplicationTask();
    StartApplicationTask(
        const QString& _version,
        const QString& _appParams );

    QByteArray serialize() const;
    bool deserialize( const QByteArray& data );
};

#endif  //APPLICATION_TASK_H
