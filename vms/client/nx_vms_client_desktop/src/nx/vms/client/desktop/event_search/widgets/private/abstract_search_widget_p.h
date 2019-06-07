#pragma once

#include "../abstract_search_widget.h"

#include <optional>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>

class QMenu;
class QLabel;
class QPropertyAnimation;
class QTimer;

namespace Ui { class AbstractSearchWidget; }
namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {

class SearchLineEdit;
class BusyIndicatorModel;
class ConcatenationListModel;

class AbstractSearchWidget::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

    AbstractSearchWidget* const q;
    nx::utils::ImplPtr<Ui::AbstractSearchWidget> ui;

public:
    Private(AbstractSearchWidget* q, AbstractSearchListModel* model);
    virtual ~Private() override;

    AbstractSearchListModel* model() const;
    EventRibbon* view() const;

    bool isAllowed() const;
    void setAllowed(bool value);

    void goToLive();

    Controls relevantControls() const;
    void setRelevantControls(Controls value);

    Period selectedPeriod() const;
    QnTimePeriod currentTimePeriod() const;

    Cameras selectedCameras() const;
    QnVirtualCameraResourceSet cameras() const;

    void selectCameras(Cameras value);
    Cameras previousCameras() const;

    QString textFilter() const;

    void setPlaceholderPixmap(const QPixmap& value);
    SelectableTextButton* createCustomFilterButton();

    void requestFetchIfNeeded();
    void resetFilters();

    void addDeviceDependentAction(
        QAction* action, const QString& mixedString, const QString& cameraString);

private:
    void setupModels();
    void setupRibbon();
    void setupViewportHeader();
    void setupPlaceholder();
    void setupTimeSelection();
    void setupCameraSelection();

    void updateCurrentTimePeriod();
    void setSelectedPeriod(Period value);
    QnTimePeriod effectiveTimePeriod() const;

    void updateCurrentCameras();
    void setSelectedCameras(Cameras value);

    void setFetchDirection(AbstractSearchListModel::FetchDirection value);
    void tryFetchMore();

    BusyIndicatorModel* relevantIndicatorModel() const;
    void handleItemCountChanged();
    void handleFetchFinished();

    void updateDeviceDependentActions();
    void updatePlaceholderVisibility();

    QString currentDeviceText() const;

private:
    const QScopedPointer<AbstractSearchListModel> m_mainModel;
    const QScopedPointer<BusyIndicatorModel> m_headIndicatorModel;
    const QScopedPointer<BusyIndicatorModel> m_tailIndicatorModel;
    const QScopedPointer<ConcatenationListModel> m_visualModel;

    QToolButton* const m_togglePreviewsButton;
    QToolButton* const m_toggleFootersButton;
    QLabel* const m_itemCounterLabel;

    SearchLineEdit* const m_textFilterEdit;

    const QScopedPointer<QTimer> m_dayChangeTimer;
    const QScopedPointer<nx::utils::PendingOperation> m_fetchMoreOperation;

    QnTimePeriod m_timelineSelection;
    Period m_period = Period::all;
    Cameras m_cameras = Cameras::all;
    QnTimePeriod m_currentTimePeriod = QnTimePeriod::anytime();
    QnVirtualCameraResourceSet m_currentCameras;

    Period m_previousPeriod = Period::all;
    QHash<Period, QAction*> m_timeSelectionActions;

    QMetaObject::Connection m_currentCameraConnection;
    Cameras m_previousCameras = Cameras::all;
    QHash<Cameras, QAction*> m_cameraSelectionActions;

    bool m_placeholderVisible = false;
    QPointer<QPropertyAnimation> m_placeholderOpacityAnimation;

    struct DeviceDependentAction
    {
        const QPointer<QAction> action;
        const QString mixedString;
        const QString cameraString;
    };

    std::vector<DeviceDependentAction> m_deviceDependentActions;

    std::optional<bool> m_isAllowed; //< std::optional for lazy initialization.
};

inline uint qHash(AbstractSearchWidget::Period source)
{
    return uint(source);
}

inline uint qHash(AbstractSearchWidget::Cameras source)
{
    return uint(source);
}

} // namespace nx::vms::client::desktop
