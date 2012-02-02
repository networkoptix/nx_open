#include "user.h"

void QnUserResource::setLayouts(QnLayoutDataList layouts)
{
    m_layouts = layouts;
}

QnLayoutDataList QnUserResource::getLayouts() const
{
    return m_layouts;
}

QString QnUserResource::getPassword() const
{
    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    m_password = password;
}
