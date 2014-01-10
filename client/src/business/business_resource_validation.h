#ifndef BUSINESS_RESOURCE_VALIDATION_H
#define BUSINESS_RESOURCE_VALIDATION_H

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>

template<typename CheckingPolicy>
static bool isResourcesListValid(const QnResourceList &resources) {
    typedef typename CheckingPolicy::resource_type ResourceType;

    QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
    if (filtered.isEmpty())
        return CheckingPolicy::emptyListIsValid();
    foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
        if (!CheckingPolicy::isResourceValid(resource))
            return false;
    return true;
}

class QnCameraInputAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
};

class QnCameraOutputAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
};

class QnCameraMotionAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
};

class QnCameraRecordingAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
};

class QnUserEmailAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnUserEmailAllowedPolicy)
public:
    typedef QnUserResource resource_type;
    static bool isResourceValid(const QnUserResourcePtr &user);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
};

#endif // BUSINESS_RESOURCE_VALIDATION_H
