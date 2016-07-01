
#pragma once

#include <statistics/base/base_fwd.h>
#include <statistics/abstract_statistics_module.h>

class QnActionManager;
class AbstractMultimetric;
typedef QPointer<QnActionManager> QnActionManagerPtr;

class QnActionsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    void setActionManager(const QnActionManagerPtr &manager);

    QnStatisticValuesHash values() const override;

    void reset() override;

private:
    typedef QList<QnStatisticsValuesProviderPtr> ModulesList;

    QnActionManagerPtr m_actionManager;
    ModulesList m_modules;
};