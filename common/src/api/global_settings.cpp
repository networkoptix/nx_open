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
    base_type(parent),
    m_disabledVendorsAdaptor(NULL),
    m_cameraSettingsOptimizationAdaptor(NULL)
{
    assert(qnResPool);
        
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnGlobalSettings::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnGlobalSettings::at_resourcePool_resourceRemoved);
    foreach(const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnGlobalSettings::~QnGlobalSettings() {
    disconnect(qnResPool, NULL, this, NULL);
    if(m_admin)
        at_resourcePool_resourceRemoved(m_admin);
}

QString QnGlobalSettings::disabledVendors() const {
    QMutexLocker locker(&m_mutex);
    
    return m_disabledVendorsAdaptor ? m_disabledVendorsAdaptor->value() : lit("all");
}

QSet<QString> QnGlobalSettings::disabledVendorsSet() const {
    return parseDisabledVendors(disabledVendors());
}

void QnGlobalSettings::setDisabledVendors(QString disabledVendors) {
    QMutexLocker locker(&m_mutex);

    if(m_disabledVendorsAdaptor)
        m_disabledVendorsAdaptor->setValue(disabledVendors);
}

bool QnGlobalSettings::isCameraSettingsOptimizationEnabled() const {
    QMutexLocker locker(&m_mutex);

    return m_cameraSettingsOptimizationAdaptor ? m_cameraSettingsOptimizationAdaptor->value() : false;
}

void QnGlobalSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled) {
    QMutexLocker locker(&m_mutex);

    if(m_cameraSettingsOptimizationAdaptor)
        m_cameraSettingsOptimizationAdaptor->setValue(cameraSettingsOptimizationEnabled);
}

void QnGlobalSettings::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if(m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    if(!user->isAdmin())
        return;

    {
        QMutexLocker locker(&m_mutex);
        m_admin = user;
        m_disabledVendorsAdaptor.reset(new QnLexicalResourcePropertyAdaptor<QString>(m_admin, lit("disabledVendors"), lit(""), this));
        m_cameraSettingsOptimizationAdaptor.reset(new QnLexicalResourcePropertyAdaptor<bool>(m_admin, lit("cameraSettingsOptimization"), false, this));
    }

    // TODO: #Elric use valueChangedExternally!
    connect(m_disabledVendorsAdaptor,               &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::disabledVendorsChanged,              Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);

    /* Just fire the signals blindly, don't check for actual changes. */ 
    emit disabledVendorsChanged();
    emit cameraSettingsOptimizationChanged();
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if(resource == m_admin) {
        QMutexLocker locker(&m_mutex);

        m_admin.reset();
        m_disabledVendorsAdaptor.reset();
        m_cameraSettingsOptimizationAdaptor.reset();

        /* Note that we don't emit changed signals here. This is for a reason. */
    }
}
