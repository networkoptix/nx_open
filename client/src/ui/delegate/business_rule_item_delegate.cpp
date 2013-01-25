#include "business_rule_item_delegate.h"

#include <QtGui/QPushButton>

#include <ui/models/business_rules_view_model.h>

void QnBusinessRuleItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    //  bool disabled = index.data(QnBusiness::DisabledRole).toBool();

    /*     QStyleOptionViewItemV4 opt = option;
           initStyleOption(&opt, index);

           QWidget *widget = opt.widget;
           widget->setEnabled(!disabled);
           QStyle *style = widget ? widget->style() : QApplication::style();
           style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);*/

    base_type::paint(painter, option, index);
}

QSize QnBusinessRuleItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize sh = base_type::sizeHint(option, index);
    if (index.column() == QnBusiness::EventColumn || index.column() == QnBusiness::ActionColumn)
        sh.setWidth(sh.width() * 1.5);
    return sh;
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    if (index.column() == QnBusiness::SourceColumn) {
        qDebug() << "editor for index" << index.row() << index.column();
        QPushButton* btn = new QPushButton(parent);
        btn->setText(index.data().toString());
        connect(btn, SIGNAL(clicked()), this, SLOT(at_sourceButton_clicked()));
        return btn;
    }
    if (index.column() == QnBusiness::TargetColumn) {
        QPushButton* btn = new QPushButton(parent);
        btn->setText(index.data().toString());
        return btn;
    }

    return base_type::createEditor(parent, option, index);
}

void QnBusinessRuleItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  {
    base_type::initStyleOption(option, index);
    if (index.data(QnBusiness::DisabledRole).toBool()) {
        if (QStyleOptionViewItemV4 *vopt = qstyleoption_cast<QStyleOptionViewItemV4 *>(option)) {
            vopt->state = QStyle::State_None;
            vopt->features |= QStyleOptionViewItemV2::Alternate;
        }
    } else if (!index.data(QnBusiness::ValidRole).toBool()) {
        option->palette.setColor(QPalette::Highlight, QColor(255, 0, 0, 127)); //TODO: #GDM skin colors
    } else
        option->palette.setColor(QPalette::Highlight, QColor(127, 127, 127, 127));
}

void QnBusinessRuleItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    base_type::setModelData(editor, model, index);
}

void QnBusinessRuleItemDelegate::at_sourceButton_clicked() {
    qDebug() << "clicked";
    /*
QnSelectCamerasDialog dialog(this, context()); //TODO: #GDM or servers?
dialog.setSelectedResources(m_model->eventResources());
if (dialog.exec() != QDialog::Accepted)
return;
m_model->setEventResources(dialog.getSelectedResources());
     */
}
