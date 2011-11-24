#include "qnid.h"

int QnId::m_lastCustomId = FIRST_CUSTOM_ID;
QMutex QnId::m_mutex;

