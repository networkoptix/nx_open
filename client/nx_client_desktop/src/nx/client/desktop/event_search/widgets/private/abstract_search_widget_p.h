#pragma once

#include "../abstract_search_widget.h"

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>

class QMenu;
class QTimer;

namespace Ui { class AbstractSearchWidget; }
namespace nx::utils { class PendingOperation; }

namespace nx::client::desktop {

class ToolButton;
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
    const QScopedPointer<Ui::AbstractSearchWidget> ui;

public:
    Private(AbstractSearchWidget* q, AbstractSearchListModel* model);
    virtual ~Private() override;

    AbstractSearchListModel* model() const;

    void goToLive();

    Controls relevantControls() const;
    void setRelevantControls(Controls value);

    Period selectedPeriod() const;
    QnTimePeriod currentTimePeriod() const;

    Cameras selectedCameras() const;
    QnVirtualCameraResourceSet cameras() const;
    void setSingleCameraMode(bool value);

    QString textFilter() const;

    bool wholeArea() const;
    void setWholeArea(bool value);

    void setPlaceholderPixmap(const QPixmap& value);
    SelectableTextButton* createCustomFilterButton();

    void requestFetchIfNeeded();
    void resetFilters();

    void addDeviceDependentAction(
        QAction* action, const QString& mixedString, const QString& cameraString);

    friend uint qHash(Period source) { return uint(source); }
    friend uint qHash(Cameras source) { return uint(source); }

private:
    void setupModels();
    void setupRibbon();
    void setupToolbar();
    void setupPlaceholder();
    void setupTimeSelection();
    void setupCameraSelection();
    void setupAreaSelection();

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

    void updateRibbonLiveMode();

private:
    const QScopedPointer<AbstractSearchListModel> m_mainModel;
    const QScopedPointer<BusyIndicatorModel> m_headIndicatorModel;
    const QScopedPointer<BusyIndicatorModel> m_tailIndicatorModel;
    const QScopedPointer<ConcatenationListModel> m_visualModel;

    SearchLineEdit* const m_textFilterEdit;

    const QScopedPointer<QTimer> m_dayChangeTimer;
    const QScopedPointer<nx::utils::PendingOperation> m_fetchMoreOperation;

    QnTimePeriod m_timelineSelection;
    Period m_period = Period::all;
    Cameras m_cameras = Cameras::all;
    QnTimePeriod m_currentTimePeriod = QnTimePeriod::anytime();
    QnVirtualCameraResourceSet m_currentCameras;
    bool m_wholeArea = true;

    Period m_previousPeriod = Period::all;
    QHash<Period, QAction*> m_timeSelectionActions;

    QMetaObject::Connection m_currentCameraConnection;
    Cameras m_previousCameras = Cameras::all;
    QHash<Cameras, QAction*> m_cameraSelectionActions;

    struct DeviceDependentAction
    {
        const QPointer<QAction> action;
        const QString mixedString;
        const QString cameraString;
    };

    QList<DeviceDependentAction> m_deviceDependentActions;
};

} // namespace nx::client::desktop
