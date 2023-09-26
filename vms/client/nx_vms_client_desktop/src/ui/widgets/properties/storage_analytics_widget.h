// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtGui/QStandardItem>
#include <QtWidgets/QWidget>

#include <api/model/recording_stats_reply.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/storage_space_reply.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QnRecordingStatsModel;

namespace Ui { class StorageAnalyticsWidget; }
namespace nx::vms::client::desktop {
class TableView;
class SnappedScrollBar;
class WidgetAnchor;
} // namespace nx::vms::client::desktop

class QnStorageAnalyticsWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnStorageAnalyticsWidget(QWidget* parent = 0);
    virtual ~QnStorageAnalyticsWidget();

    virtual void loadDataToUi() override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

private:
    void updateDataFromServer();
    void clearCache();
    void setupTableView(nx::vms::client::desktop::TableView* table,
        nx::vms::client::desktop::TableView* totalsTable, QAbstractItemModel* model);
    nx::vms::client::desktop::TableView* currentTable() const;
    nx::vms::client::desktop::TableView* currentTotalsTable() const;
    qint64 currentForecastAveragingPeriod();

    // Starts server request.
    void querySpaceFromServer();
    void queryStatsFromServer(qint64 bitrateAveragingPeriodMs);

    bool requestsInProgress() const;
    void processRequestFinished();

    qint64 sliderPositionToBytes(int value) const;
    int bytesToSliderPosition (qint64 value) const;

    QScopedPointer<Ui::StorageAnalyticsWidget> ui;

    QnMediaServerResourcePtr m_server;

    QnRecordingStatsModel* m_model = nullptr;
    QnRecordingStatsModel* m_forecastModel = nullptr;
    int m_spaceRequestHandle = -1; //< Request handle for storage space information.
    struct StatsRequest
    {
        int handle = -1; //< Request handle for statistics.
        qint64 averagingPeriod = 0;
    } m_statsRequest[2]; //< One is always for "Storage" tab with avg=5min, second is for "Forecast".

    bool m_updating = false;
    bool m_dirty = false;
    int m_updateTimerId = 0;

    QAction* m_selectAllAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_clipboardAction = nullptr;
    Qt::MouseButton m_lastMouseButton = Qt::NoButton;
    nx::vms::client::desktop::SnappedScrollBar* m_statsTableScrollbar;
    nx::vms::client::desktop::SnappedScrollBar* m_forecastTableScrollbar;
    nx::vms::client::desktop::HintButton* m_hintButton;
    nx::vms::client::desktop::WidgetAnchor* m_hintAnchor;

    // Map from averaging period to recording stats.
    QMap<quint64, QnRecordingStatsReply> m_recordingsStatData;
    nx::vms::api::StorageSpaceDataListV1 m_availableStorages;
    qint64 m_availableStorageSpace = 0;

    void updateTotalTablesGeometry();
    void positionHintButton();

    virtual void resizeEvent(QResizeEvent*) override; //< Use this to resize table columns; accepts null.
    virtual void showEvent(QShowEvent*) override; //< Resizes columns when shown.
    virtual void hideEvent(QHideEvent*) override;
    virtual void timerEvent(QTimerEvent*) override;

private:
    void atReceivedStats(
        bool success, int handle, const QnRecordingStatsReply& data);
    void atReceivedSpaceInfo(QnMediaServerResourcePtr server,
        bool success, int handle, const nx::vms::api::StorageSpaceReply& data);

private slots:
    void atEventsGrid_customContextMenuRequested(const QPoint&);
    void atClipboardAction_triggered();
    void atExportAction_triggered();
    void atMouseButtonRelease(QObject*, QEvent* event);
    void atForecastParamsChanged();
    void atAveragingPeriodChanged();
    void atPageChanged();
};
