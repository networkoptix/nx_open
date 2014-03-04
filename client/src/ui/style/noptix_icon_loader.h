#ifndef QN_NOPTIX_ICON_LOADER_H
#define QN_NOPTIX_ICON_LOADER_H

#include <QtCore/QObject>
#include <QtGui/QIcon>

class QnSkin;

class QnNoptixIconLoader: public QObject {
    Q_OBJECT
    typedef QObject base_type;

public:
    QnNoptixIconLoader(QObject *parent = NULL);
    virtual ~QnNoptixIconLoader();

    QIcon polish(const QIcon &icon);
    QIcon load(const QString &name, const QString &checkedName = QString());

private:
    QHash<QString, QIcon> m_iconByKey;
    QSet<qint64> m_cacheKeys;
};


#endif // QN_NOPTIX_ICON_LOADER_H
