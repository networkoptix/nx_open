#include "user_resource.h"

QnUserResource::QnUserResource(): base_type()
{
    addFlag(QnResource::user);
}

void QnUserResource::setLayouts(QnLayoutResourceList layouts)
{
    m_layouts = layouts;
}

QnLayoutResourceList QnUserResource::getLayouts() const
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

void QnUserResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if(localOther) {
        setPassword(localOther->getPassword());

        // TODO: Doesn't work because other user's layouts are not in the pool. Ivan?
        // It may be a good idea to replace ptr-list with id-list.
        setLayouts(localOther->getLayouts());
    }
}