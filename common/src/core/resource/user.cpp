#include "user.h"
#include <utils/common/warnings.h>

QnUserResource::QnUserResource(): base_type()
{
    addFlag(QnResource::user);
}

void QnUserResource::setLayouts(const QnLayoutDataList &layouts)
{
    m_layouts = layouts;
}

const QnLayoutDataList &QnUserResource::getLayouts() const
{
    return m_layouts;
}

void QnUserResource::addLayout(const QnLayoutDataPtr &layout) 
{
    if(m_layouts.contains(layout)) {
        qnWarning("Given layout '%1' is already in %2's layouts list.", layout->getName(), getName());
        return;
    }

    m_layouts.push_back(layout);
}

void QnUserResource::removeLayout(const QnLayoutDataPtr &layout) 
{
    m_layouts.removeOne(layout); /* Removing a layout that is not there is not an error. */
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