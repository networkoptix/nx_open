#include "calendar_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QTableView>
#include <QtGui/QHeaderView>
#include <QtGui/QToolButton>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/color_transformations.h>
#include <ui/style/globals.h>

namespace {
    const QColor selectionColor = withAlpha(qnGlobals->selectionColor(), 192);

    const QColor backgroundColor(24, 24, 24, 0);
    //const QColor backgroundColor(0, 0, 0, 0);

    const QColor recordingColor(32, 128, 32, 255);
    //const QColor recordingColor(16, 64, 16, 255);

    const QColor motionColor(128, 0, 0, 255);
    //const QColor motionColor(64, 0, 0, 255);

    const QColor separatorColor(0, 0, 0, 255);

    enum {
        DAY = 1000 * 60 * 60 * 24
    };

} // anonymous namespace


QnCalendarWidget::QnCalendarWidget():
    QCalendarWidget(),
    m_tableView(0)
{
    /* Month button's drop-down menu doesn't work well with graphics scene, so we simply remove it. */
    QToolButton *monthButton = findChild<QToolButton *>(QLatin1String("qt_calendar_monthbutton"));
    if(monthButton)
        monthButton->setMenu(NULL);

    QWidget *yearEdit = findChild<QWidget *>(QLatin1String("qt_calendar_yearedit"));
    QnSingleEventEater *contextMenuEater = new QnSingleEventEater(Qn::AcceptEvent, yearEdit);
    contextMenuEater->setEventType(QEvent::ContextMenu);
    yearEdit->installEventFilter(contextMenuEater);

    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setVerticalHeaderFormat(QnCalendarWidget::NoVerticalHeader);

    m_tableView = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    Q_ASSERT(m_tableView);
    m_tableView->horizontalHeader()->setMinimumSectionSize(18);
    QObject::connect(m_tableView, SIGNAL(changeDate(const QDate&, bool)), this, SIGNAL(dateClicked(const QDate&)));

    QWidget* navBarBackground = findChild<QWidget *>(QLatin1String("qt_calendar_navigationbar"));
    navBarBackground->setBackgroundRole(QPalette::Window);
}

void QnCalendarWidget::setCurrentTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods)
{
    bool oldEmpty = isEmpty();
    m_currentTimeStorage.setPeriods(type, periods);
    bool newEmpty = isEmpty();
    if (newEmpty != oldEmpty)
        emit emptyChanged();

    update();
}

void QnCalendarWidget::setSyncedTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods) {
    // TODO: #GDM Copypasta. Move to separate function (updateEmpty()) and add m_empty field.
    bool oldEmpty = isEmpty();
    m_syncedTimeStorage.setPeriods(type, periods);
    bool newEmpty = isEmpty();
    if (newEmpty != oldEmpty)
        emit emptyChanged();

    update();
}

void QnCalendarWidget::setSelectedWindow(quint64 windowStart, quint64 windowEnd) {

    bool modified = false;
    if (windowStart != (quint64)m_window.startTimeMs)
        modified = windowStart/DAY != (quint64)m_window.startTimeMs/DAY;
    if (!modified && windowEnd != (quint64)m_window.endTimeMs())
        modified = windowEnd/DAY !=(quint64)m_window.endTimeMs()/DAY;

    m_window.startTimeMs = windowStart;
    m_window.durationMs = windowEnd - windowStart;

    if (modified)
        update();
}

bool QnCalendarWidget::isEmpty() {
    for(int type = 0; type < Qn::TimePeriodRoleCount; type++)
        if (!m_currentTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty()
                ||
            !m_syncedTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty())
            return false;
    return true;
}

void QnCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate &date) const {
    QDateTime dt(date);
    QnTimePeriod current(dt.toMSecsSinceEpoch(), DAY);

    QnScopedPainterBrushRollback brushRollback(painter);
    Q_UNUSED(brushRollback);

    bool isCurrent = !current.intersected(m_window).isEmpty();

    /* Draw background. */
    {
        QBrush brush = painter->brush();

        if (m_currentTimeStorage.periods(Qn::MotionRole).intersects(current)) {
            brush.setColor(motionColor);
            brush.setStyle(Qt::SolidPattern);
        } else if (m_currentTimeStorage.periods(Qn::RecordingRole).intersects(current)) {
            brush.setColor(recordingColor);
            brush.setStyle(Qt::SolidPattern);
        } else {
            brush.setColor(backgroundColor);
            brush.setStyle(Qt::SolidPattern);
        }
        painter->fillRect(rect, brush);

        if (m_syncedTimeStorage.periods(Qn::MotionRole).intersects(current)) {
            brush.setColor(motionColor);
            brush.setStyle(Qt::BDiagPattern);
        } else if (m_syncedTimeStorage.periods(Qn::RecordingRole).intersects(current)) {
            brush.setColor(recordingColor);
            brush.setStyle(Qt::BDiagPattern);
        }
        painter->fillRect(rect, brush);
    }

    QnScopedPainterPenRollback penRollback(painter);
    Q_UNUSED(penRollback);

    painter->setBrush(Qt::NoBrush);

    /* Selection frame. */
    if (isCurrent) {
        painter->setPen(QPen(selectionColor, 3, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->drawRect(rect.adjusted(2, 2, -2, -2));
    }

    /* Common black frame. */
    painter->setPen(QPen(separatorColor, 1));
    painter->drawRect(rect);

    /* Draw text. */
    {
        QnScopedPainterFontRollback fontRollback(painter);
        Q_UNUSED(fontRollback);

        QFont font = painter->font();
        font.setPixelSize(12);

        QString text = QString::number(date.day());
        QColor color;
        if (date < this->minimumDate() || date > this->maximumDate()) {
            color = palette().color(QPalette::Disabled, QPalette::Text);
        } else {
            color = palette().color(QPalette::Active, QPalette::Text);
            font.setBold(true);
        }

        painter->setPen(QPen(color, 1));
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignCenter, text);
    }
}
