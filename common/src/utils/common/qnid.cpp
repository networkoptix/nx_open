#include "qnid.h"

int QnId::m_lastCustomId = 1000000000;
QMutex QnId::m_mutex;

