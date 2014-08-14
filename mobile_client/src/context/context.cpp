#include "context.h"

#include "connection_manager.h"

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this))
{
}

QnContext::~QnContext() {}



