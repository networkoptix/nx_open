#pragma once

#include <QtCore/QString>
#include <QtCore/QQueue>
#include <utils/common/connective.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

class QueueFileWriter;

class QnWriterPool : public Connective<QObject>, public Singleton<QnWriterPool>
{
    Q_OBJECT
public:
    typedef QMap<QnUuid, QueueFileWriter*> WritersMap;

    QnWriterPool(QObject *parent = nullptr);
    ~QnWriterPool();

    QueueFileWriter* getWriter(const QnUuid& writePoolId);
    WritersMap getAllWriters();
private:
    QnMutex m_mutex;
    WritersMap m_writers;
};
