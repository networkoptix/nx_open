#include "day_time_widget.h"

#include <QVBoxLayout>
#include <QTableWidget>
#include <QLabel>

#include <utils/common/variant.h>

class QnDayTimeTableWidget: public QTableWidget {
    typedef QTableWidget base_type;
public:
    QnDayTimeTableWidget(QWidget *parent = NULL): 
        base_type(parent) 
    {
        setTabKeyNavigation(false);
        setShowGrid(true);
        verticalHeader()->setVisible(false);
        verticalHeader()->setResizeMode(QHeaderView::Stretch);
        verticalHeader()->setMinimumSectionSize(0);
        verticalHeader()->setClickable(false);
        horizontalHeader()->setVisible(false);
        horizontalHeader()->setResizeMode(QHeaderView::Stretch);
        horizontalHeader()->setMinimumSectionSize(0);
        horizontalHeader()->setClickable(false);
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


QnDayTimeWidget::QnDayTimeWidget(QWidget *parent):
    base_type(parent)
{
    m_headerLabel = new QLabel(this);
    
    m_tableWidget = new QnDayTimeTableWidget(this);
    m_tableWidget->setRowCount(4);
    m_tableWidget->setColumnCount(6);
    for(int i = 0; i < 24; i++) {
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setText(lit("%1").arg(i, 2, 10, lit('0')));
        item->setData(Qt::UserRole, i);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_tableWidget->setItem(i / 6, i % 6, item);
    }

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(m_headerLabel);
    layout->addWidget(m_tableWidget);
    setLayout(layout);
    
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_tableWidget);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(m_tableWidget, SIGNAL(itemClicked(QTableWidgetItem *)), this, SLOT(at_tableWidget_itemClicked(QTableWidgetItem *)));
}

QnDayTimeWidget::~QnDayTimeWidget() {
    return;
}

void QnDayTimeWidget::at_tableWidget_itemClicked(QTableWidgetItem *item) {
    int hour = qvariant_cast<int>(item->data(Qt::UserRole), -1);
    if(hour < 0)
        return;

    emit clicked(QTime(hour, 0, 0, 0));
}


