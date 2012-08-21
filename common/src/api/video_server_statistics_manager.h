#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QMetaType>
#include <QtCore/QString>

class QnVideoServerStatisticsManager: QObject{
    Q_OBJECT

public:
    QnVideoServerStatisticsManager(QObject *parent = NULL): QObject(parent){}
};

//Q_DECLARE_METATYPE(QnVideoServerStatisticsManager)

#endif // QN_STATISTICS_MANAGER
