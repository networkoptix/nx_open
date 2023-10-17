// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QVector>
#include <QtQuick/QQuickImageProvider>

#include <analytics/common/object_metadata.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/event_search/utils/analytics_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/event_search/utils/common_object_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")

namespace nx::analytics::db { struct ObjectTrack; }

namespace nx::vms::client::desktop {

class AnalyticsSearchSetup;
class CommonObjectSearchSetup;
class WindowContext;

class RightPanelModelsAdapter: public QIdentityProxyModel
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context
        READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)

    Q_PROPERTY(bool isAllowed READ isAllowed NOTIFY allowanceChanged)

    Q_PROPERTY(nx::vms::client::desktop::RightPanelModelsAdapter::Type type
        READ type WRITE setType NOTIFY typeChanged)

    Q_PROPERTY(nx::vms::client::desktop::CommonObjectSearchSetup* commonSetup READ commonSetup
        CONSTANT)

    Q_PROPERTY(nx::vms::client::desktop::AnalyticsSearchSetup* analyticsSetup READ analyticsSetup
        NOTIFY analyticsSetupChanged)

    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(QString itemCountText READ itemCountText NOTIFY itemCountChanged)
    Q_PROPERTY(bool isConstrained READ isConstrained NOTIFY isConstrainedChanged)

    Q_PROPERTY(bool placeholderRequired READ isPlaceholderRequired
        NOTIFY placeholderRequiredChanged)

    Q_PROPERTY(bool previewsEnabled READ previewsEnabled WRITE setPreviewsEnabled
        NOTIFY previewsEnabledChanged)

    /**
     * Whether this model is currently active.
     * Activity can be changed from outside (e.g. by user choosing a corresponding UI tab)
     * or from inside (for example by toggling Motion Search mode).
     */
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

    Q_PROPERTY(QVector<nx::vms::client::desktop::RightPanel::EventCategory> eventCategories
        READ eventCategories CONSTANT)

    Q_PROPERTY(QAbstractItemModel* analyticsEvents READ analyticsEvents
        NOTIFY analyticsEventsChanged)

    using base_type = QIdentityProxyModel;

public:
    RightPanelModelsAdapter(QObject* parent = nullptr);
    virtual ~RightPanelModelsAdapter() override;

    WindowContext* context() const;
    void setContext(WindowContext* value);

    enum class Type
    {
        invalid = -1,
        notifications,
        motion,
        bookmarks,
        events,
        analytics
    };
    Q_ENUM(Type)

    Type type() const;
    void setType(Type value);

    bool isOnline() const;
    bool isAllowed() const;

    bool active() const;
    void setActive(bool value);

    CommonObjectSearchSetup* commonSetup() const;
    AnalyticsSearchSetup* analyticsSetup() const;

    int itemCount() const;
    QString itemCountText() const;
    bool isConstrained() const;
    bool isPlaceholderRequired() const;

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool value);

    Q_INVOKABLE bool removeRow(int row);

    Q_INVOKABLE void setAutoClosePaused(int row, bool value);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setFetchDirection(nx::vms::client::desktop::RightPanel::FetchDirection value);
    Q_INVOKABLE void requestFetch(bool immediately = false);
    Q_INVOKABLE bool canFetch() const;
    Q_INVOKABLE bool fetchInProgress() const;

    Q_INVOKABLE void setLivePaused(bool value);

    Q_INVOKABLE void click(int row, int button, int modifiers);
    Q_INVOKABLE void doubleClick(int row);
    Q_INVOKABLE void startDrag(int row, const QPoint& pos, const QSize& size);
    Q_INVOKABLE void activateLink(int row, const QString& link);
    Q_INVOKABLE void showContextMenu(int row, const QPoint& globalPos,
        bool withStandardInteraction);

    Q_INVOKABLE void addCameraToLayout();

    Q_INVOKABLE void showEventLog();

    Q_INVOKABLE void showOnLayout(int row);

    QVector<RightPanel::EventCategory> eventCategories() const;
    QAbstractItemModel* analyticsEvents() const;

    void setHighlightedTimestamp(std::chrono::microseconds value);
    void setHighlightedResources(const QSet<QnResourcePtr>& value);

    static QStringList flattenAttributeList(const analytics::AttributeList& source);

    static void registerQmlTypes();

signals:
    void contextChanged();
    void typeChanged();
    void dataNeeded();
    void liveAboutToBeCommitted();
    void asyncFetchStarted(nx::vms::client::desktop::RightPanel::FetchDirection direction);
    void fetchCommitStarted(nx::vms::client::desktop::RightPanel::FetchDirection direction);
    void fetchFinished();
    void analyticsSetupChanged();
    void itemCountChanged();
    void isConstrainedChanged();
    void isOnlineChanged();
    void activeChanged();
    void placeholderRequiredChanged();
    void previewsEnabledChanged();
    void allowanceChanged();
    void analyticsEventsChanged();

    void clicked(const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void doubleClicked(const QModelIndex& index);
    void dragStarted(const QModelIndex& index, const QPoint& pos, const QSize& size);
    void linkActivated(const QModelIndex& index, const QString& link);
    void contextMenuRequested(
        const QModelIndex& index,
        const QPoint& globalPos,
        bool withStandardInteraction,
        QWidget* parent);
    void pluginActionRequested(const QnUuid& engineId, const QString& actionTypeId,
        const nx::analytics::db::ObjectTrack& track, const QnVirtualCameraResourcePtr& camera);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

class RightPanelImageProvider: public QQuickImageProvider
{
public:
    RightPanelImageProvider(): QQuickImageProvider(Image) {}
    virtual QImage requestImage(const QString& id, QSize* size, const QSize& /*requestedSize*/);
};

} // namespace nx::vms::client::desktop
