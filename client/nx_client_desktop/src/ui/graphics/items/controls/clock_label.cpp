#include "clock_label.h"

#include <QtCore/QDateTime>

#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <client/client_settings.h>

#include <ui/common/palette.h>
#include <ui/style/nx_style.h>
#include <utils/common/synctime.h>
#include <translation/datetime_formatter.h>


QnClockDataProvider::QnClockDataProvider(QObject *parent) :
    QObject(parent),
    m_timer(new QTimer(this)),
    m_clockType(localSystemClock)
{

    connect(m_timer, &QTimer::timeout, this, [this](){
        emit timeChanged(
            datetime::toString(m_clockType == serverClock
                ? qnSyncTime->currentMSecsSinceEpoch()
                : QDateTime::currentMSecsSinceEpoch(),
                datetime::Format::hh_mm_ss));
    });
    m_timer->start(100);
}

QnClockDataProvider::~QnClockDataProvider() = default;

void QnClockDataProvider::setClockType( ClockType clockType )
{
    m_clockType = clockType;
}

QnClockLabel::QnClockLabel(QGraphicsItem *parent):
    base_type(parent)
{
    QFont font;
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    setFont(font);

    m_provider = new QnClockDataProvider(this);
    connect(m_provider, &QnClockDataProvider::timeChanged, this, &GraphicsLabel::setText);

    m_serverTimeAction = new QAction(tr("Server Time"), this);
    addAction(m_serverTimeAction);
    m_localTimeAction = new QAction(tr("Local System Time"), this);
    addAction(m_localTimeAction);
}

QnClockLabel::~QnClockLabel() = default;

void QnClockLabel::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
#ifdef _DEBUG
    //displaying clock type selection menu
    QMenu menu;
    menu.addActions(actions());
    QAction* selectedAction = menu.exec(event->screenPos());
    if (selectedAction == m_serverTimeAction)
        m_provider->setClockType(QnClockDataProvider::serverClock);
    else if(selectedAction == m_localTimeAction)
        m_provider->setClockType(QnClockDataProvider::localSystemClock);
#else
    base_type::contextMenuEvent( event );
#endif
}
