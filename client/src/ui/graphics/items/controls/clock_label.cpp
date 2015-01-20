#include "clock_label.h"

#include <QtCore/QDateTime>

#include <client/client_settings.h>

#include <ui/common/palette.h>
#include <utils/common/synctime.h>



QnClockDataProvider::QnClockDataProvider(const QString fixedFormat, QObject *parent) :
    QObject(parent),
    m_timer(new QTimer(this)),
    m_format(qnSettings->isClock24Hour() ? Hour24 : Hour12),
    m_showWeekDay(qnSettings->isClockWeekdayOn()),
    m_showDateAndMonth(qnSettings->isClockDateOn()),
    m_showSeconds(qnSettings->isClockSecondsOn()),
    m_formatString(fixedFormat),
    m_clockType(localSystemClock)
{
    if (m_formatString.isEmpty()) {
        connect(qnSettings->notifier(QnClientSettings::CLOCK_24HOUR),     SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
        connect(qnSettings->notifier(QnClientSettings::CLOCK_WEEKDAY),    SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
        connect(qnSettings->notifier(QnClientSettings::CLOCK_DATE),       SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
        connect(qnSettings->notifier(QnClientSettings::CLOCK_SECONDS),    SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));

        updateFormatString();
    }

    connect(m_timer, &QTimer::timeout, this, [this](){
        emit timeChanged(
            m_clockType == serverClock
            ? QDateTime::fromMSecsSinceEpoch( qnSyncTime->currentMSecsSinceEpoch() ).toString( m_formatString )
            : QDateTime::currentDateTime().toString(m_formatString) );
    });
    m_timer->start(100);
}

QnClockDataProvider::~QnClockDataProvider() {
    return;
}

void QnClockDataProvider::setClockType( ClockType clockType )
{
    m_clockType = clockType;
}

void QnClockDataProvider::updateFormatString() {
    // TODO: #GDM #Common field values are not updated from settings.
    // We probably don't need this complexity anyway.

    m_formatString = QString();
    if (m_showWeekDay)
        m_formatString += lit("ddd ");
    if (m_showDateAndMonth)
        m_formatString += lit("MMM d ");
    m_formatString += lit("hh:mm");
    if (m_showSeconds)
        m_formatString += lit(":ss");
    if (m_format == Hour12)
        m_formatString += lit(" AP");
}

QnClockLabel::QnClockLabel(QGraphicsItem *parent):
    base_type(parent)
{
    init(lit("hh:mm:ss"));
}

QnClockLabel::QnClockLabel(const QString &format, QGraphicsItem *parent):
    base_type(parent)
{
    init(format);
}

QnClockLabel::~QnClockLabel() {
    return;
}

void QnClockLabel::init(const QString &format) {
    QFont font;
    font.setPixelSize(30);
    setFont(font);

    setPaletteColor(this, QPalette::WindowText, QColor(64, 130, 180, 128));

    m_provider = new QnClockDataProvider(format, this);
    connect( m_provider, &QnClockDataProvider::timeChanged, this, &GraphicsLabel::setText );


    m_serverTimeAction = new QAction( tr( "Server time" ), this );
    addAction( m_serverTimeAction );
    m_localTimeAction = new QAction( tr( "Local system time" ), this );
    addAction( m_localTimeAction );
}

void QnClockLabel::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
#ifdef _DEBUG
    //displaying clock type selection menu
    QMenu menu;
    menu.addActions( actions() );
    QAction* selectedAction = menu.exec( event->screenPos() );
    if( selectedAction == m_serverTimeAction )
        m_provider->setClockType( QnClockDataProvider::serverClock );
    else if( selectedAction == m_localTimeAction )
        m_provider->setClockType( QnClockDataProvider::localSystemClock );
#else
    base_type::contextMenuEvent( event );
#endif
}
