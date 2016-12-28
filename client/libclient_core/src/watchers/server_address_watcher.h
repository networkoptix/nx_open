#pragma once

#include <QtCore/QObject>

class QnServerAddressWatcher : public QObject
{
    Q_OBJECT
public:
    explicit QnServerAddressWatcher(QObject* parent = nullptr);

private:
    QList<QUrl> m_urls;
};
