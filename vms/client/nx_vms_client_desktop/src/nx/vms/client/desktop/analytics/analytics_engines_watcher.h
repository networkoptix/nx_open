#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class AnalyticsEnginesWatcher: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    struct AnalyticsEngineInfo
    {
        QnUuid id;
        QString name;
        QJsonObject settingsModel;
    };

    AnalyticsEnginesWatcher(QObject* parent = nullptr);
    virtual ~AnalyticsEnginesWatcher() override;

    QHash<QnUuid, AnalyticsEngineInfo> engineInfos() const;
    AnalyticsEngineInfo engineInfo(const QnUuid& engineId) const;

signals:
    void engineAdded(const QnUuid& engineId, const AnalyticsEngineInfo& info);
    void engineRemoved(const QnUuid& engineId);
    void engineNameChanged(const QnUuid& engineId, const QString& name);
    void engineSettingsModelChanged(const QnUuid& engineId, const QJsonObject& settingsModel);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
