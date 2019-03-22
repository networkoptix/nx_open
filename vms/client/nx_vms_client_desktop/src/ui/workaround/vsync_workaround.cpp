#include "vsync_workaround.h"

#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QWidget>

namespace {

using namespace std::chrono;

constexpr int kFpsLimit = 60;
constexpr milliseconds kTimeBetweenUpdates = 1000ms / kFpsLimit;

} // namespace

QnVSyncWorkaround::QnVSyncWorkaround(QWidget* watched, QObject* parent):
    QObject(parent),
    m_watched(watched)
{
    const auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, m_watched,
        [this]()
        {
            m_paintAllowed = true;
            m_watched->update();
        });

    timer->start(kTimeBetweenUpdates);
    m_watched->installEventFilter(this);
}

bool QnVSyncWorkaround::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_watched)
        return false;

    switch (event->type())
    {
        case QEvent::Paint:
            if (!m_paintAllowed)
                return true;
            m_paintAllowed = false;
            return false;

        case QEvent::Resize:
            m_paintAllowed = true;
            return false;

        default:
            return false;
    }

}
