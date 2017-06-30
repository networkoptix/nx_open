#pragma once

#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <ui/style/icon.h>

class QnSkin;

class QnNoptixIconLoader: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnNoptixIconLoader(QObject* parent = nullptr);
    virtual ~QnNoptixIconLoader();

    QIcon polish(const QIcon& icon);
    QIcon load(
        const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::SuffixesList* suffixes = nullptr);

private:
    void loadIconInternal(
        const QString& key,
        const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::SuffixesList* suffixes = nullptr);

private:
    QHash<QString, QIcon> m_iconByKey;
    QSet<qint64> m_cacheKeys;
};
