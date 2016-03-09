
#pragma once

#include <utils/common/connective.h>
#include <finders/abstract_systems_finder.h>
#include <watchers/cloud_status_watcher.h>

class QnCloudSystemsFinder : public Connective<QnAbstractSystemsFinder>
{
    Q_OBJECT
    typedef Connective<QnAbstractSystemsFinder> base_type;
    
public:
    QnCloudSystemsFinder(QObject *parent = nullptr);

    virtual ~QnCloudSystemsFinder();

public: // overrides
    SystemDescriptionList systems() const override;

private:
    void onCloudStatusChanged(QnCloudStatusWatcher::Status status);

    void onCloudSystemsChanged(const QnCloudSystemList &systems);

    void onCloudError(QnCloudStatusWatcher::ErrorCode error);

private:
    SystemDescriptionList m_systems;
};