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
namespace nx::client::desktop { class TableView; }

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
    void setupTableView(nx::client::desktop::TableView* table, QAbstractItemModel* model);
    nx::client::desktop::TableView* currentTable() const;
    qint64 currentForecastAveragingPeriod();

    QnRecordingStatsReply getForecastData(qint64 extraSizeBytes);

    // Removes "foreign" and hidden cameras and aggregates their data in one pseudo-camera.
    QnRecordingStatsReply filterStatsReply(const QnRecordingStatsReply& cameraStats);

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
    //QnRecordingStatsReply m_recordingsStatData;
    QVector<QnStorageSpaceData> m_availableStorages;

    // Forecast-related data.
    struct ForecastDataPerCamera
    {
        QnCamRecordingStatsData stats; //< Forecasted statistics.
        bool expand = false; //< Expand archive for that camera in the forecast.
        int minDays = 0; //< Cached camera 'minDays' value.
        int maxDays = 0; //< Cached camera 'maxDays' value.
        qint64 byterate = 0; //< How may bytes camera gives per second (bytes/second).
    };

    struct ForecastData
    {
        qint64 totalSpace = 0; //< Total space for all storages.
        QVector<ForecastDataPerCamera> cameras; //< Camera list by server.
    };

    QnRecordingStatsReply doForecast(ForecastData forecastData);
    void spendData(ForecastData& forecastData, qint64 needSeconds,
        std::function<bool (const ForecastDataPerCamera& stats)> predicate);

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
