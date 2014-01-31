#include "global_settings.h"

#include <cassert>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include "resource_property_adaptor.h"

QnGlobalSettings::QnGlobalSettings(QObject *parent): 
    base_type(parent),
    m_cameraAutoDiscoveryAdaptor(NULL),
    m_cameraSettingsOptimizationAdaptor(NULL)
{
    assert(qnResPool);
        
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnGlobalSettings::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnGlobalSettings::at_resourcePool_resourceRemoved);
    foreach(const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceRemoved(resource);
}

QnGlobalSettings::~QnGlobalSettings() {
    disconnect(qnResPool, NULL, this, NULL);
    if(m_admin)
        at_resourcePool_resourceRemoved(m_admin);
}

bool QnGlobalSettings::isCameraAutoDiscoveryEnabled() const {
    QMutexLocker locker(&m_mutex);
    
    return m_cameraAutoDiscoveryAdaptor ? m_cameraAutoDiscoveryAdaptor->value() : true;
}

void QnGlobalSettings::setCameraAutoDiscoveryEnabled(bool cameraAutoDiscoveryEnabled) {
    QMutexLocker locker(&m_mutex);

    if(m_cameraAutoDiscoveryAdaptor)
        m_cameraAutoDiscoveryAdaptor->setValue(cameraAutoDiscoveryEnabled);
}

bool QnGlobalSettings::isCameraSettingsOptimizationEnabled() const {
    QMutexLocker locker(&m_mutex);

    return m_cameraSettingsOptimizationAdaptor ? m_cameraSettingsOptimizationAdaptor->value() : true;
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

    if(user->getName() != lit("admin"))
        return;

    m_admin = user;
    m_cameraAutoDiscoveryAdaptor.reset(new QnLexicalResourcePropertyAdaptor<bool>(m_admin, lit("cameraAutoDiscovery"), true, this));
    m_cameraSettingsOptimizationAdaptor.reset(new QnLexicalResourcePropertyAdaptor<bool>(m_admin, lit("cameraSettingsOptimization"), true, this));

    connect(m_cameraAutoDiscoveryAdaptor,           &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraAutoDiscoveryChanged,          Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);

    /* Just fire the signals blindly, don't check for actual changes. */ 
    emit cameraAutoDiscoveryChanged();
    emit cameraSettingsOptimizationChanged();
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if(resource == m_admin) {
        QMutexLocker locker(&m_mutex);

        m_admin.reset();
        m_cameraAutoDiscoveryAdaptor.reset();
        m_cameraSettingsOptimizationAdaptor.reset();

        /* Note that we don't emit changed signals here. This is for a reason. */
    }
}
