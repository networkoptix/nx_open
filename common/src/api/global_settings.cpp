#include "global_settings.h"

#include <cassert>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include "resource_property_adaptor.h"

namespace {
    QSet<QString> parseDisabledVendors(QString disabledVendors) {
        QStringList disabledVendorList;
        if (disabledVendors.contains(lit(";")))
            disabledVendorList = disabledVendors.split(lit(";"));
        else
            disabledVendorList = disabledVendors.split(lit(" "));

        QStringList updatedVendorList;        
        for (int i = 0; i < disabledVendorList.size(); ++i) {
            if (!disabledVendorList[i].trimmed().isEmpty()) {
                updatedVendorList << disabledVendorList[i].trimmed();
            }
        }

        return updatedVendorList.toSet();
    }
}

QnGlobalSettings::QnGlobalSettings(QObject *parent): 
    base_type(parent)
{
    assert(qnResPool);
    
    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(lit("disabledVendors"), QString(), this);
    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(lit("cameraSettingsOptimization"), true, this);
    m_systemNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(lit("systemName"), QString(), this);

    connect(m_disabledVendorsAdaptor,               &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::disabledVendorsChanged,              Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);
    connect(m_systemNameAdaptor,                    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::systemNameChanged,                   Qt::QueuedConnection);

    connect(qnResPool,                              &QnResourcePool::resourceAdded,                     this,   &QnGlobalSettings::at_resourcePool_resourceAdded);
    connect(qnResPool,                              &QnResourcePool::resourceRemoved,                   this,   &QnGlobalSettings::at_resourcePool_resourceRemoved);
    foreach(const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnGlobalSettings::~QnGlobalSettings() {
    disconnect(qnResPool, NULL, this, NULL);
    if(m_admin)
        at_resourcePool_resourceRemoved(m_admin);
}

QString QnGlobalSettings::disabledVendors() const {
    return m_disabledVendorsAdaptor->value();
}

QSet<QString> QnGlobalSettings::disabledVendorsSet() const {
    return parseDisabledVendors(disabledVendors());
}

void QnGlobalSettings::setDisabledVendors(QString disabledVendors) {
    m_disabledVendorsAdaptor->setValue(disabledVendors);
}

bool QnGlobalSettings::isCameraSettingsOptimizationEnabled() const {
    return m_cameraSettingsOptimizationAdaptor->value();
}

void QnGlobalSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled) {
    m_cameraSettingsOptimizationAdaptor->setValue(cameraSettingsOptimizationEnabled);
}

QString QnGlobalSettings::systemName() const {
    return m_systemNameAdaptor->value();
}

void QnGlobalSettings::setSystemName(const QString& systemName) {
    m_systemNameAdaptor->setValue(systemName);
}

void QnGlobalSettings::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if(m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    if(!user->isAdmin())
        return;

    m_admin = user;
    m_disabledVendorsAdaptor->setResource(user);;
    m_cameraSettingsOptimizationAdaptor->setResource(user);;
    m_systemNameAdaptor->setResource(user);
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if(resource == m_admin) {
        QMutexLocker locker(&m_mutex);

        m_admin.reset();
        m_disabledVendorsAdaptor->setResource(QnResourcePtr());
        m_cameraSettingsOptimizationAdaptor->setResource(QnResourcePtr());
    }
}
