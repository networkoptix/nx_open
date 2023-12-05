// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <QtCore/QDate>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/desktop/event_search/widgets/abstract_search_widget.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

class QMenu;
class QLabel;
class QPropertyAnimation;
class QTimer;

namespace Ui { class AbstractSearchWidget; }
namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::core { class ConcatenationListModel; }

namespace nx::vms::client::desktop {

class SearchLineEdit;
class BusyIndicatorModel;
class CommonObjectSearchSetup;
class PlaceholderWidget;

class AbstractSearchWidget::Private:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QWidget;

    AbstractSearchWidget* const q;
    nx::utils::ImplPtr<Ui::AbstractSearchWidget> ui;

public:
    Private(AbstractSearchWidget* q, core::AbstractSearchListModel* model);
    virtual ~Private() override;

    core::AbstractSearchListModel* model() const;
    EventRibbon* view() const;

    bool isAllowed() const;
    void setAllowed(bool value);

    void goToLive();

    Controls relevantControls() const;
    void setRelevantControls(Controls value);

    CommonObjectSearchSetup* commonSetup() const;

    void setPlaceholderPixmap(const QPixmap& value);
    SelectableTextButton* createCustomFilterButton();

    void addFilterWidget(QWidget* widget, Qt::Alignment alignment); //< Ownership is taken.

    /** Returns fetch direction for the update or nothing if there is no need to do it. */
    std::optional<core::EventSearch::FetchDirection> getFetchDirection();
    void requestFetchIfNeeded();
    void resetFilters();

    void addDeviceDependentAction(
        QAction* action, const QString& mixedString, const QString& cameraString);

    void setPreviewToggled(bool value);
    bool previewToggled() const;

    void setFooterToggled(bool value);
    bool footerToggled() const;

    void addSearchAction(QAction* action);

    std::chrono::microseconds currentCentralPointUs() const;

private:
    void setupModels();
    void setupRibbon();
    void setupViewportHeader();
    void setupPlaceholder();
    void setupTimeSelection();
    void setupCameraSelection();

    void tryFetchData();

    BusyIndicatorModel* relevantIndicatorModel(core::EventSearch::FetchDirection direction) const;
    void handleItemCountChanged();
    void handleFetchFinished();

    void updateDeviceDependentActions();
    void updatePlaceholderVisibility();

    QString currentDeviceText() const;
    QString singleDeviceText(
        const QString& baseText, const QnVirtualCameraResourcePtr& device) const;
    QString deviceButtonText(core::EventSearch::CameraSelection selection) const;

    void setCurrentDate(const QDateTime& value);

private:
    const QScopedPointer<core::AbstractSearchListModel> m_mainModel;
    const QScopedPointer<BusyIndicatorModel> m_headIndicatorModel;
    const QScopedPointer<BusyIndicatorModel> m_tailIndicatorModel;
    const QScopedPointer<core::ConcatenationListModel> m_visualModel;

    PlaceholderWidget* m_placeholderWidget;
    QToolButton* const m_togglePreviewsButton;
    QToolButton* const m_toggleFootersButton;
    QLabel* const m_itemCounterLabel;

    SearchLineEdit* const m_textFilterEdit;

    CommonObjectSearchSetup* const m_commonSetup;

    const QScopedPointer<nx::utils::PendingOperation> m_fetchDataOperation;

    QHash<core::EventSearch::TimeSelection, QAction*> m_timeSelectionActions;

    QMetaObject::Connection m_currentCameraConnection;
    QHash<core::EventSearch::CameraSelection, QAction*> m_cameraSelectionActions;

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

    QDateTime m_currentDate; //< Either client or server time, depending on the local settings.
};

} // namespace nx::vms::client::desktop
