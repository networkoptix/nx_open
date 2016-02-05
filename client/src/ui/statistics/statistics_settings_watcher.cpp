
#include "statistics_settings_watcher.h"

#include <QtCore/QTimer>

#include <utils/common/model_functions.h>

#include <utils/network/http/asynchttpclient.h>

//QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStatisticsSettings, (json)(eq), QnStatisticsSettings_Fields)

namespace
{
    enum
    {
        kDefaultLimit = 1
        , kDefaultStoreDays = 30
    };
}

QnStatisticsSettings::QnStatisticsSettings()
    : limit(kDefaultLimit)
    , storeDays(kDefaultStoreDays)
    , filters()
{}

//

QnStatisticsSettingsWatcher::QnStatisticsSettingsWatcher(QObject *parent)
    : base_type(parent)
    , m_settings()
    , m_updateTimer(new QTimer())
{
    enum { kUpdatePeriodMs = 30 * 60 * 1000 };    // 30 minutes update period
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(kUpdatePeriodMs);
    m_updateTimer->start();

    connect(m_updateTimer, &QTimer::timeout
        , this, &QnStatisticsSettingsWatcher::updateSettings);

    updateSettings();
}

QnStatisticsSettingsWatcher::~QnStatisticsSettingsWatcher()
{}

QnStatisticsSettings QnStatisticsSettingsWatcher::settings() const
{
    return m_settings;
}

void QnStatisticsSettingsWatcher::updateSettings()
{
//    bool deserialized = false;
//    m_settings = QJson::deserialized(
//        , QnStatisticsSettings(), &deserialized);
}

