#ifndef __QN_IOPORTS_ACTUAL_MODEL_H_
#define __QN_IOPORTS_ACTUAL_MODEL_H_

#include <nx_ec/ec_api.h>
#include "ioports_view_model.h"

// TODO: #GDM #Business move out from global namespace.
enum QnIOPortsActualModelChange {
    RulesLoaded,
    RuleSaved
};

/**
* Sync model data with actual DB data if DB was changed outside model
*/
class QnIOPortsActualModel: public QnIOPortsViewModel
{
    Q_OBJECT

    typedef QnIOPortsViewModel base_type;
public:
    QnIOPortsActualModel(QObject *parent = 0);
signals:
    void beforeModelChanged();
    void afterModelChanged(QnIOPortsActualModelChange change, bool ok);
private slots:
    void at_propertyChanged(const QnResourcePtr& res, const QString& key);
};

#endif // __QN_IOPORTS_ACTUAL_MODEL_H_
