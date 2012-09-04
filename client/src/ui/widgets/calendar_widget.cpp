#include "calendar_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QTableView>
#include <QtGui/QHeaderView>
#include <QtGui/QToolButton>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>

#include <ui/common/color_transformations.h>
#include <ui/style/globals.h>

namespace {
    const QColor selectionColor = withAlpha(qnGlobals->selectionColor(), 192);

    const QColor backgroundColor(24, 24, 24, 0);
    //const QColor backgroundColor(0, 0, 0, 0);

    const QColor denseRecordingColor(32, 255, 32, 255);
    const QColor recordingColor(32, 128, 32, 255);
    //const QColor recordingColor(16, 64, 16, 255);

    const QColor denseMotionColor(255, 0, 0, 255);
    const QColor motionColor(128, 0, 0, 255);
    //const QColor motionColor(64, 0, 0, 255);

    const QColor separatorColor(0, 0, 0, 255);

    enum {
        DAY = 1000 * 60 * 60 * 24
    };

} // anonymous namespace


QnCalendarWidget::QnCalendarWidget():
    m_tableView(0),
    m_empty(true),
    m_currentTime(0),
    m_currentWidgetIsCentral(false)
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
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    m_tableView = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    m_tableView->horizontalHeader()->setMinimumSectionSize(18);
    QObject::connect(m_tableView, SIGNAL(changeDate(const QDate&, bool)), this, SIGNAL(dateClicked(const QDate&)));

    m_tableView->viewport()->installEventFilter(this);

    QPalette palette = m_tableView->palette();
    palette.setColor(QPalette::Highlight, QColor(0, 0, 0, 255));
    m_tableView->setPalette(palette);

    QWidget* navBarBackground = findChild<QWidget *>(QLatin1String("qt_calendar_navigationbar"));
    navBarBackground->setBackgroundRole(QPalette::Window);
}

void QnCalendarWidget::setCurrentTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods)
{
    m_currentTimeStorage.setPeriods(type, periods);
    updateEmpty();
    update();
}

void QnCalendarWidget::setSyncedTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods) {
    m_syncedTimeStorage.setPeriods(type, periods);
    updateEmpty();
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

void QnCalendarWidget::setCurrentWidgetIsCentral(bool currentWidgetIsCentral){
    if (m_currentWidgetIsCentral == currentWidgetIsCentral)
        return;

    m_currentWidgetIsCentral = currentWidgetIsCentral;
    update();
}

bool QnCalendarWidget::isEmpty() {
    return m_empty;
}

bool QnCalendarWidget::eventFilter(QObject *watched, QEvent *event){
    if (event->type() == QEvent::Paint && watched == m_tableView->viewport())
        m_currentTime = qnSyncTime->currentMSecsSinceEpoch();
    return base_type::eventFilter(watched, event);
}

void QnCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate &date) const {
    QnTimePeriod period(QDateTime(date).toMSecsSinceEpoch(), DAY); 
    if (period.startTimeMs > m_currentTime)
        period = QnTimePeriod();

    QnScopedPainterBrushRollback brushRollback(painter);
    Q_UNUSED(brushRollback);

    bool inWindow = !period.intersected(m_window).isEmpty();

    /* Draw background. */
    {
        QBrush brush = painter->brush();

        QColor bgcolor;
        if (m_currentWidgetIsCentral && m_currentTimeStorage.periods(Qn::MotionRole).intersects(period)) {
            bgcolor = motionColor;
        } else if (m_currentWidgetIsCentral && m_currentTimeStorage.periods(Qn::RecordingRole).intersects(period)) {
            bgcolor = recordingColor;
        } else {
            bgcolor = backgroundColor;
        }
        brush.setColor(bgcolor);
        brush.setStyle(Qt::SolidPattern);
        painter->fillRect(rect, brush);

        // TODO: #GDM logic based on color comparisons is a bad practice. Introduce additional booleans if needed, but don't compare colors.
        if (bgcolor != motionColor && m_syncedTimeStorage.periods(Qn::MotionRole).intersects(period)) {
            brush.setColor(denseMotionColor);
            brush.setStyle(Qt::Dense6Pattern);
            painter->fillRect(rect, brush);
        } else if (bgcolor != recordingColor && m_syncedTimeStorage.periods(Qn::RecordingRole).intersects(period)) {
            brush.setColor(denseRecordingColor);
            brush.setStyle(Qt::Dense6Pattern);
            painter->fillRect(rect, brush);
        }
    }

    QnScopedPainterPenRollback penRollback(painter);
    Q_UNUSED(penRollback);

    painter->setBrush(Qt::NoBrush);

    /* Selection frame. */
    if (inWindow) {
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

void QnCalendarWidget::updateEmpty() {
    bool value = true;
    for(int type = 0; type < Qn::TimePeriodRoleCount; type++) {
        if (!m_currentTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty() ||
            !m_syncedTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty())
        {
            value = false;
            break;
        }
    }
    if (m_empty == value)
        return;

    m_empty = value;
    emit emptyChanged();
}

void QnCalendarWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the calendar widget. */
}

void QnCalendarWidget::mousePressEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnCalendarWidget::mouseReleaseEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}
