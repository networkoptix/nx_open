#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

#include "analytics_objects_visualization_mode.h"

namespace nx::vms::client::desktop {

/**
 * Manages analytics objects visualization mode for cameras (layout items), saves it locally.
 * Connection entries are saved by system id to make sure we will clean all non-existent layout
 * items sometimes.
 */
class NX_VMS_CLIENT_DESKTOP_API AnalyticsObjectsVisualizationManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using Mode = AnalyticsObjectsVisualizationMode;
    static constexpr auto kDefaultMode = Mode::tabOnly;
    static constexpr auto kNonDefaultMode = Mode::always;
    static_assert(kDefaultMode != kNonDefaultMode);

    explicit AnalyticsObjectsVisualizationManager(QObject* parent = nullptr);
    virtual ~AnalyticsObjectsVisualizationManager() override;

    Mode mode(const QnLayoutItemIndex& item) const;
    void setMode(const QnLayoutItemIndex& item, Mode value);

    Mode mode(const QnLayoutItemIndexList& items) const;
    void setMode(const QnLayoutItemIndexList& items, Mode value);

    QString cacheDirectory() const;
    void setCacheDirectory(const QString& value);

    /** Load stored settings for the given system, drop other. */
    void switchLocalSystemId(const QnUuid& localSystemId);

    /** Save items for the local system, cleanup non-existing. */
    void saveData(const QnUuid& localSystemId, QnResourcePool* resourcePool) const;

signals:
    void modeChanged(const QnLayoutItemIndex& index, Mode value);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
