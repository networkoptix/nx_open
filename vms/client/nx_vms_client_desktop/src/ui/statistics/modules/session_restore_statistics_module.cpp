// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_restore_statistics_module.h"

QnSessionRestoreStatisticsModule::QnSessionRestoreStatisticsModule(QObject* parent):
    base_type(parent)
{
}

QnStatisticValuesHash QnSessionRestoreStatisticsModule::values() const
{
    return m_values;
}

void QnSessionRestoreStatisticsModule::reset()
{
    m_values.clear();
}

void QnSessionRestoreStatisticsModule::report(QString name, QString value)
{
    if (m_capturing)
        m_values.insert(name, value);
}

void QnSessionRestoreStatisticsModule::startCapturing(bool fullRestore)
{
    m_values["mechanism"] = fullRestore ? "full" : "soft";
    m_capturing = true;
}

void QnSessionRestoreStatisticsModule::stopCapturing()
{
    m_capturing = false;
}
