#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

class QnServerAddressWatcher : public QObject
{
    Q_OBJECT
public:
    typedef QList<QUrl> UrlsList;
    typedef std::function<void (const UrlsList& values)> Setter;
    typedef std::function<UrlsList ()> Getter;

    explicit QnServerAddressWatcher(QObject* parent = nullptr);

    QnServerAddressWatcher(
        const Getter& getter,
        const Setter& setter,
        QObject* parent = nullptr);

    void setAccessors(const Getter& getter, const Setter& setter);

private:
    void shrinkUrlsList();

private:
    QList<QUrl> m_urls;
};
