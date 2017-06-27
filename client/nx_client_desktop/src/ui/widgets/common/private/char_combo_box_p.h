#pragma once

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QListView>

// -------------------------------------------------------------------------- //
// QComboBoxListView
// -------------------------------------------------------------------------- //
/**
 * Combo box list view as implemented in <tt>qcombobox_p.h</tt>.
 *
 * Note that it is important for this class to have metainformation generated
 * as some styles include special handling of <tt>QComboBoxListView</tt> class name.
 */
class QComboBoxListView : public QListView
{
    Q_OBJECT
public:
    QComboBoxListView(QComboBox *cmb = 0) : combo(cmb) {}

protected:
    void resizeEvent(QResizeEvent *event)
    {
        resizeContents(viewport()->width(), contentsSize().height());
        QListView::resizeEvent(event);
    }

    QStyleOptionViewItem viewOptions() const
    {
        QStyleOptionViewItem option = QListView::viewOptions();
        option.showDecorationSelected = true;
        if (combo)
            option.font = combo->font();
        return option;
    }

    void paintEvent(QPaintEvent *e)
    {
        if (combo) {
            QStyleOptionComboBox opt;
            opt.initFrom(combo);
            opt.editable = combo->isEditable();
            if (combo->style()->styleHint(QStyle::SH_ComboBox_Popup, &opt, combo)) {
                //we paint the empty menu area to avoid having blank space that can happen when scrolling
                QStyleOptionMenuItem menuOpt;
                menuOpt.initFrom(this);
                menuOpt.palette = palette();
                menuOpt.state = QStyle::State_None;
                menuOpt.checkType = QStyleOptionMenuItem::NotCheckable;
                menuOpt.menuRect = e->rect();
                menuOpt.maxIconWidth = 0;
                menuOpt.tabWidth = 0;
                QPainter p(viewport());
                combo->style()->drawControl(QStyle::CE_MenuEmptyArea, &menuOpt, &p, this);
            }
        }
        QListView::paintEvent(e);
    }

private:
    QComboBox *combo;
};


// -------------------------------------------------------------------------- //
// QnCharComboBoxListView
// -------------------------------------------------------------------------- //
class QnCharComboBoxListView: public QComboBoxListView {
    Q_OBJECT
    typedef QComboBoxListView base_type;

public:
    QnCharComboBoxListView(QComboBox *comboBox = NULL): QComboBoxListView(comboBox) {}

    virtual void keyboardSearch(const QString &search) override {
        if(!search.isEmpty()) {
            QModelIndex currentIndex = this->currentIndex();
            base_type::keyboardSearch(QString()); /* Clear search data. */
            setCurrentIndex(currentIndex);
        }

        base_type::keyboardSearch(search);
    }
};
