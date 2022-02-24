// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <statistics/abstract_statistics_module.h>
#include <statistics/base/base_fwd.h>

class QnSessionRestoreStatisticsModule: public QnAbstractStatisticsModule
{
    Q_OBJECT
    using base_type = QnAbstractStatisticsModule;

public:
    QnSessionRestoreStatisticsModule(QObject* parent = nullptr);
    virtual ~QnSessionRestoreStatisticsModule() = default;

    virtual QnStatisticValuesHash values() const override;
    virtual void reset() override;

    void report(QString name, QString value);
    void startCapturing(bool fullRestore);
    void stopCapturing();

private:
    QnStatisticValuesHash m_values;
    bool m_capturing = false;
};
