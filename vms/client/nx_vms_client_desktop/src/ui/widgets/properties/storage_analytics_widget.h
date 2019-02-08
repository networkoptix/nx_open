#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/model/recording_stats_reply.h>
#include <api/model/storage_space_reply.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnRecordingStatsModel;

namespace Ui { class StorageAnalyticsWidget; }
namespace nx::vms::client::desktop { class TableView; }

class QnStorageAnalyticsWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QnAbstractPreferencesWidget>;

public:
    QnStorageAnalyticsWidget(QWidget* parent = 0);
    virtual ~QnStorageAnalyticsWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

private:
    void updateDataFromServer();
    void clearCache();
    void setupTableView(nx::vms::client::desktop::TableView* table, QAbstractItemModel* model);
    nx::vms::client::desktop::TableView* currentTable() const;
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

    QAction* m_selectAllAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_clipboardAction = nullptr;
    Qt::MouseButton m_lastMouseButton = Qt::NoButton;

    // Map from averaging period to recording stats.
    QMap<quint64, QnRecordingStatsReply> m_recordingsStatData;
    QnStorageSpaceDataList m_availableStorages;
    qint64 m_availableStorageSpace = 0;

    virtual void resizeEvent(QResizeEvent*) override; //< Use this to resize table columns; accepts null.
    virtual void showEvent(QShowEvent*) override; //< Resizes columns when shown.

private slots:
    void atReceivedStats(int status, const QnRecordingStatsReply& data, int requestNum);
    void atReceivedSpaceInfo(int status, const QnStorageSpaceReply& data, int requestNum);
    void atEventsGrid_customContextMenuRequested(const QPoint&);
    void atClipboardAction_triggered();
    void atExportAction_triggered();
    void atMouseButtonRelease(QObject*, QEvent* event);
    void atForecastParamsChanged();
    void atAveragingPeriodChanged();
};
