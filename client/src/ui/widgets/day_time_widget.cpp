#include "day_time_widget.h"

#include <QtCore/QLocale>

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>

#include <utils/common/variant.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

#include <ui/common/palette.h>
#include <ui/delegates/calendar_item_delegate.h>

namespace {
    enum {
        HOUR = 60 * 60 * 1000
    };

    enum { kHeaderLabelMargin = 2 };
}


// -------------------------------------------------------------------------- //
// QnDayTimeItemDelegate
// -------------------------------------------------------------------------- //
class QnDayTimeItemDelegate: public QnCalendarItemDelegate {
public:
    QnDayTimeItemDelegate(QnDayTimeWidget *dayTimeWidget): QnCalendarItemDelegate(dayTimeWidget), m_dayTimeWidget(dayTimeWidget) {}

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        m_dayTimeWidget->paintCell(painter, option.rect, index.data(Qt::UserRole).toTime());
    }

private:
    QnDayTimeWidget *m_dayTimeWidget;
};


// -------------------------------------------------------------------------- //
// QnDayTimeTableWidget
// -------------------------------------------------------------------------- //
class QnDayTimeTableWidget: public QTableWidget {
    typedef QTableWidget base_type;
public:
    QnDayTimeTableWidget(QWidget *parent = NULL): 
        base_type(parent) 
    {
        setTabKeyNavigation(false);
        setShowGrid(false);
        verticalHeader()->setVisible(false);
        verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        verticalHeader()->setMinimumSectionSize(0);
        verticalHeader()->setSectionsClickable(false);
        horizontalHeader()->setVisible(false);
        horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        horizontalHeader()->setMinimumSectionSize(0);
        horizontalHeader()->setSectionsClickable(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSelectionBehavior(QAbstractItemView::SelectItems);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setFrameStyle(QFrame::NoFrame);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

protected:
    virtual QSize sizeHint() const override {
        return minimumSizeHint();
    }

    virtual QSize minimumSizeHint() const override {
        int w = 0, h = 0;
        
        for(int row = 0; row < rowCount(); row++)
            h += sizeHintForRow(row);

        for(int col = 0; col < columnCount(); col++)
            w += sizeHintForColumn(col);

        return QSize(w, h);
    }
};


// -------------------------------------------------------------------------- //
// QnDayTimeWidget
// -------------------------------------------------------------------------- //
QnDayTimeWidget::QnDayTimeWidget(QWidget *parent):
    base_type(parent),
    m_timeFormat(lit("hh:mm")),
    m_localOffset(0)
{
    m_headerLabel = new QLabel(this);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    
    m_tableWidget = new QnDayTimeTableWidget(this);
    //setPaletteColor(m_tableWidget, QPalette::Highlight, QColor(0, 0, 0, 255));
    m_tableWidget->setRowCount(4);
    m_tableWidget->setColumnCount(6);
    for(int i = 0; i < 24; i++) {
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setData(Qt::UserRole, QTime(i, 0, 0, 0));
        m_tableWidget->setItem(i / 6, i % 6, item);
    }

    m_delegate = new QnDayTimeItemDelegate(this);
    m_tableWidget->setItemDelegate(m_delegate);

    QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    signalizer->setEventType(QEvent::Paint);
    m_tableWidget->viewport()->installEventFilter(signalizer);
    connect(signalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(updateCurrentTime()));

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(kHeaderLabelMargin, kHeaderLabelMargin
        , kHeaderLabelMargin, kHeaderLabelMargin);
    headerLayout->addWidget(m_headerLabel, 0);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    layout->addLayout(headerLayout, 0);
    layout->addWidget(m_tableWidget, 1);
    setLayout(layout);
    
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_tableWidget);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(m_tableWidget, SIGNAL(itemClicked(QTableWidgetItem *)), this, SLOT(at_tableWidget_itemClicked(QTableWidgetItem *)));

    setDate(QDate::currentDate());
    updateCurrentTime();
    updateEnabled();
}

QnDayTimeWidget::~QnDayTimeWidget() {
    return;
}

QDate QnDayTimeWidget::date() {
    return m_date;
}

void QnDayTimeWidget::setDate(const QDate &date) {
    if(m_date == date)
        return;

    m_date = date;

    updateEnabled();
    updateHeaderText();
}

void QnDayTimeWidget::setPrimaryTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods) {
    m_primaryPeriodStorage.setPeriods(type, periods);
    update();
}

void QnDayTimeWidget::setSecondaryTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods) {
    m_secondaryPeriodStorage.setPeriods(type, periods);
    update();
}

void QnDayTimeWidget::setEnabledWindow(quint64 windowStart, quint64 windowEnd) {
    m_enabledPeriod = QnTimePeriod(windowStart, windowEnd - windowStart);

    if(m_enabledPeriod.startTimeMs >= m_enabledHoursPeriod.startTimeMs && m_enabledPeriod.startTimeMs < m_enabledHoursPeriod.startTimeMs + HOUR &&
        m_enabledPeriod.endTimeMs() <= m_enabledHoursPeriod.endTimeMs() && m_enabledPeriod.endTimeMs() > m_enabledHoursPeriod.endTimeMs() - HOUR)
        return; /* Ok, no update needed. */

    QDateTime hourWindowStart = QDateTime::fromMSecsSinceEpoch(windowStart);
    hourWindowStart.setTime(QTime(hourWindowStart.time().hour(), 0, 0, 0));
    QDateTime hourWindowEnd = QDateTime::fromMSecsSinceEpoch(windowEnd + HOUR - 1);
    hourWindowEnd.setTime(QTime(hourWindowEnd.time().hour(), 0, 0, 0));
    
    qint64 start = hourWindowStart.toMSecsSinceEpoch();
    qint64 end = hourWindowEnd.toMSecsSinceEpoch();

    QnTimePeriod hoursWindow = QnTimePeriod(start, end - start);
    if(m_enabledHoursPeriod == hoursWindow)
        return;

    m_enabledHoursPeriod = hoursWindow;
    updateEnabled();
}

void QnDayTimeWidget::setSelectedWindow(quint64 windowStart, quint64 windowEnd) {
    m_selectedPeriod = QnTimePeriod(windowStart, windowEnd - windowStart);

    if(m_selectedPeriod.startTimeMs >= m_selectedHoursPeriod.startTimeMs && m_selectedPeriod.startTimeMs < m_selectedHoursPeriod.startTimeMs + HOUR &&
        m_selectedPeriod.endTimeMs() <= m_selectedHoursPeriod.endTimeMs() && m_selectedPeriod.endTimeMs() > m_selectedHoursPeriod.endTimeMs() - HOUR)
        return; /* Ok, no update needed. */

    QDateTime hourWindowStart = QDateTime::fromMSecsSinceEpoch(windowStart);
    hourWindowStart.setTime(QTime(hourWindowStart.time().hour(), 0, 0, 0));
    QDateTime hourWindowEnd = QDateTime::fromMSecsSinceEpoch(windowEnd + HOUR - 1);
    hourWindowEnd.setTime(QTime(hourWindowEnd.time().hour(), 0, 0, 0));
    
    qint64 start = hourWindowStart.toMSecsSinceEpoch();
    qint64 end = hourWindowEnd.toMSecsSinceEpoch();

    QnTimePeriod hoursWindow = QnTimePeriod(start, end - start);
    if(m_selectedHoursPeriod == hoursWindow)
        return;

    m_selectedHoursPeriod = hoursWindow;
    update();
}

void QnDayTimeWidget::paintCell(QPainter *painter, const QRect &rect, const QTime &time) {
    QnTimePeriod period(QDateTime(m_date, time).toMSecsSinceEpoch(), HOUR); 
    if (period.startTimeMs - m_localOffset > m_currentTime)
        period = QnTimePeriod();

    m_delegate->paintCell(
        painter, 
        palette(), 
        rect, 
        period, 
        m_localOffset,
        m_enabledPeriod, 
        m_selectedPeriod, 
        m_primaryPeriodStorage, 
        m_secondaryPeriodStorage, 
        time.toString(m_timeFormat)
    );
}

void QnDayTimeWidget::updateHeaderText() {
    /* Note that the format is the same as in time slider date bar. */
    m_headerLabel->setText(lit("<b>%1</b>").arg(m_date.toString(lit("dd MMMM yyyy"))));
}

void QnDayTimeWidget::updateCurrentTime() {
    m_currentTime = qnSyncTime->currentMSecsSinceEpoch();
}

void QnDayTimeWidget::updateEnabled() {
    for(int row = 0; row < m_tableWidget->rowCount(); row++) {
        for(int col = 0; col < m_tableWidget->columnCount(); col++) {
            QTableWidgetItem *item = m_tableWidget->item(row, col);
            QTime time = item->data(Qt::UserRole).toTime();
            QnTimePeriod period(QDateTime(m_date, time).toMSecsSinceEpoch() - m_localOffset, HOUR);

            item->setFlags(m_enabledPeriod.intersects(period) ? (Qt::ItemIsSelectable | Qt::ItemIsEnabled) : Qt::NoItemFlags);
        }
    }
}

void QnDayTimeWidget::at_tableWidget_itemClicked(QTableWidgetItem *item) {
    if(!(item->flags() & Qt::ItemIsEnabled))
        return;

    QTime time = item->data(Qt::UserRole).toTime();
    if(!time.isValid())
        return;

    emit timeClicked(time);
}

qint64 QnDayTimeWidget::localOffset() const {
    return m_localOffset;
}

void QnDayTimeWidget::setLocalOffset(qint64 localOffset) {
    if(m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;

    m_tableWidget->update();
    updateEnabled();
}

int QnDayTimeWidget::headerHeight() const
{
    enum { kVerticalMarginsCount = 2};  /// Upper and lower margins
    return m_headerLabel->height() + kHeaderLabelMargin * kVerticalMarginsCount;
}
