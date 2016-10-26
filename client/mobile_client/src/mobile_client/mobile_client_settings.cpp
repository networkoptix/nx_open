#include "mobile_client_settings.h"

#include <QtCore/QSettings>

#include <utils/common/scoped_value_rollback.h>

#include "mobile_client_app_info.h"

QnMobileClientSettings::QnMobileClientSettings(QObject* parent) :
    base_type(parent),
    m_settings(new QSettings(this)),
    m_loading(true)
{
    init();
    load();
    setThreadSafe(true);
    m_loading = false;
}

void QnMobileClientSettings::load()
{
    updateFromSettings(m_settings);
}

void QnMobileClientSettings::save()
{
    submitToSettings(m_settings);
}

bool QnMobileClientSettings::isWritable() const
{
    return m_settings->isWritable();
}

bool QnMobileClientSettings::isLiteClientModeEnabled() const
{
    switch (static_cast<LiteModeType>(liteMode()))
    {
    case LiteModeType::LiteModeEnabled:
        return true;
    case LiteModeType::LiteModeAuto:
        return QnMobileClientAppInfo::defaultLiteMode();
    default:
        break;
    }
    return false;
}

void QnMobileClientSettings::updateValuesFromSettings(
        QSettings* settings,
        const QList<int>& ids)
{
    QN_SCOPED_VALUE_ROLLBACK(&m_loading, true);

    base_type::updateValuesFromSettings(settings, ids);
}

QVariant QnMobileClientSettings::readValueFromSettings(
        QSettings* settings,
        int id,
        const QVariant& defaultValue)
{
    return base_type::readValueFromSettings(settings, id, defaultValue);
}

void QnMobileClientSettings::writeValueToSettings(
    QSettings* settings,
    int id,
    const QVariant& value) const
{
    switch (id)
    {
        /* Temporary options. Not to be written. */
        case BasePath:
        case LiteMode:
        case TestMode:
        case InitialTest:
        case StartupParameters:
            break;

        default:
            base_type::writeValueToSettings(settings, id, value);
            break;
    }
}

QnPropertyStorage::UpdateStatus QnMobileClientSettings::updateValue(
        int id,
        const QVariant& value)
{
    const auto status = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if (!m_loading && status == Changed)
        writeValueToSettings(m_settings, id, this->value(id));

    return status;
}
