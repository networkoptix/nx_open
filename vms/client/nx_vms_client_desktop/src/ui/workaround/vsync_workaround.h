#pragma once

#include <QtCore/QObject>

class QWidget;

/**
 *  This class ensures that update() events are passed to widget
 *  no more often than once in a 16ms (~60 fps)
  **/
class QnVSyncWorkaround: public QObject
{
    Q_OBJECT

public:
    QnVSyncWorkaround(QWidget* watched, QObject* parent = nullptr);

protected:
    virtual bool eventFilter(QObject* object, QEvent* vent) override;

private:
    QWidget* const m_watched;
    bool m_paintAllowed = false;
};
