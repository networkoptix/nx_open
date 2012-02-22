#include "user_resource.h"
#include <utils/common/warnings.h>

QnUserResource::QnUserResource()
    : base_type(),
      m_isAdmin(false)
{
    addFlags(QnResource::user);
}

void QnUserResource::setLayouts(const QnLayoutResourceList &layouts)
{
    QMutexLocker locker(&m_mutex);

    while(!m_layouts.empty())
        removeLayout(m_layouts.front()); /* Removing an item from QList's front is constant time. */

    foreach(const QnLayoutResourcePtr &layout, layouts)
        addLayout(layout);
}

QnLayoutResourceList QnUserResource::getLayouts() const
{
    QMutexLocker locker(&m_mutex);

    return m_layouts;
}

void QnUserResource::addLayout(const QnLayoutResourcePtr &layout) 
{
    QMutexLocker locker(&m_mutex);

    if(m_layouts.contains(layout)) {
        qnWarning("Given layout '%1' is already in %2's layouts list.", layout->getName(), getName());
        return;
    }

    if(layout->getParentId() != QnId() && layout->getParentId() != getId()) {
        qnWarning("Given layout '%1' is already in other user's layouts list.", layout->getName());
        return;
    }

    m_layouts.push_back(layout);
    layout->setParentId(getId());
}

void QnUserResource::removeLayout(const QnLayoutResourcePtr &layout) 
{
    QMutexLocker locker(&m_mutex);

    m_layouts.removeOne(layout); /* Removing a layout that is not there is not an error. */
    layout->setParentId(QnId());
}


QString QnUserResource::getUniqueId() const
{
    return getName();
}

QString QnUserResource::getPassword() const
{
    QMutexLocker locker(&m_mutex);

    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    QMutexLocker locker(&m_mutex);

    m_password = password;
}

bool QnUserResource::isAdmin() const
{
    return m_isAdmin;
}

void QnUserResource::setAdmin(bool admin)
{
    m_isAdmin = admin;
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