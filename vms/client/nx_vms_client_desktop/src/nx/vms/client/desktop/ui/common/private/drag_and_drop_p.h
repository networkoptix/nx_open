#pragma once

#include <QtCore/QMimeData>
#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace nx::vms::client::desktop {

class DragMimeDataWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* watched READ watched WRITE setWatched NOTIFY watchedChanged)
    Q_PROPERTY(QMimeData* mimeData READ mimeData NOTIFY mimeDataChanged)

public:
    QObject* watched() const;
    void setWatched(QObject* value);

    QMimeData* mimeData() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setMimeData(QMimeData* value);

signals:
    void watchedChanged(QPrivateSignal);
    void mimeDataChanged(QPrivateSignal);

private:
    QPointer<QObject> m_watched;
    QPointer<QMimeData> m_mimeData;
};

} // namespace nx::vms::client::desktop
