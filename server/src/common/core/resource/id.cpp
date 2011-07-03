#include "id.h"

static unsigned long counter = 0;
static QMutex  counter_cs;

QString QnId::generateNewId()
{
    QMutexLocker mtx(&counter_cs);
    return QString::number(++counter);
}

