#include "audit_manager.h"

static QnAuditManager* m_globalInstance = 0;

QnAuditManager* QnAuditManager::instance()
{
    return m_globalInstance;
}

QnAuditManager::QnAuditManager()
{
    assert( m_globalInstance == nullptr );
    m_globalInstance = this;
}
