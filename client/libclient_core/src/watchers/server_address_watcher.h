#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

class QnServerAddressWatcher : public QObject
{
    Q_OBJECT
public:
    explicit QnServerAddressWatcher(QObject* parent = nullptr);

private:
    void shrinkUrlsList();

private:
    QList<QUrl> m_urls;
};
