#include "eve_app.h"
#include "base/log.h"

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
			cl_log.log(QString("FileOpen: ") + static_cast<QFileOpenEvent*>(event)->file(), cl_logALWAYS);
			emit messageReceived(static_cast<QFileOpenEvent*>(event)->file());
//			QString fileName = static_cast<QFileOpenEvent*>(event)->file();
//            m_fileNames.append(fileName);
//            cl_log.log(fileName, cl_logALWAYS);
            return true;
		}
        default:
            return QtSingleApplication::event(event);
    }
}
