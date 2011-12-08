#include "eve_app.h"

#include "utils/common/log.h"

EveApplication::EveApplication(int &argc, char **argv)
    : QtSingleApplication(argc, argv, true)
{
}

QStringList EveApplication::getFileNames() const
{
    return m_fileNames;
}

bool EveApplication::event(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::FileOpen:
    {
        QString fileName = static_cast<QFileOpenEvent*>(event)->file();
        cl_log.log(QLatin1String("FileOpen: ") + fileName, cl_logALWAYS);
        emit messageReceived(fileName);
//        m_fileNames.append(fileName);
//        cl_log.log(fileName, cl_logALWAYS);
        return true;
    }
    default:
        break;
    }

    return QtSingleApplication::event(event);
}
