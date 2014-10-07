#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_colorTheme(new QnColorTheme(this))
{
}

QnContext::~QnContext() {}
