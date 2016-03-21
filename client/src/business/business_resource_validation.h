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
    else if (filtered.size() > 1 && !CheckingPolicy::multiChoiceListIsValid())
        return false;

    foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
        if (!CheckingPolicy::isResourceValid(resource))
            return false;
    return true;
}

class QnCameraInputPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraOutputPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnExecPtzPresetPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnExecPtzPresetPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
};

class QnCameraMotionPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraRecordingPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

typedef QnCameraRecordingPolicy QnBookmarkActionPolicy;

class QnUserEmailPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnUserEmailPolicy)
public:
    typedef QnUserResource resource_type;
    static bool isResourceValid(const QnUserResourcePtr &user);
    static QString getText(const QnResourceList &resources, const bool detailed = true, const QStringList &additional = QStringList());
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

#endif // BUSINESS_RESOURCE_VALIDATION_H
