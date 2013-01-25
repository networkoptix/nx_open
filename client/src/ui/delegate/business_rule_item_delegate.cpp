#include "business_rule_item_delegate.h"

#include <ui/dialogs/select_cameras_dialog.h>
#include <ui/models/business_rules_view_model.h>


QnIndexedDialogButton::QnIndexedDialogButton(QWidget *parent):
    QPushButton(parent)
{
    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QnResourceList QnIndexedDialogButton::resources() {
    return m_resources;
}

void QnIndexedDialogButton::setResources(QnResourceList resources) {
    m_resources = resources;
}

void QnIndexedDialogButton::at_clicked() {
    QnSelectCamerasDialog dialog(this); //TODO: #GDM servers dialog?
    dialog.setSelectedResources(m_resources);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_resources = dialog.getSelectedResources();
}

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
        QnIndexedDialogButton* btn = new QnIndexedDialogButton(parent);
        btn->setFlat(true);
        btn->setText(index.data().toString());
        return btn;
    }
    if (index.column() == QnBusiness::TargetColumn) {
        QnIndexedDialogButton* btn = new QnIndexedDialogButton(parent);
        btn->setFlat(true);
        btn->setText(index.data().toString());
        return btn;
    }

    return base_type::createEditor(parent, option, index);
}

void QnBusinessRuleItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  {
    base_type::initStyleOption(option, index);
    if (index.data(QnBusiness::DisabledRole).toBool()) {
        if (QStyleOptionViewItemV4 *vopt = qstyleoption_cast<QStyleOptionViewItemV4 *>(option)) {
            vopt->state &= ~QStyle::State_Enabled;
         //   vopt->features |= QStyleOptionViewItemV2::Alternate;
        }
        option->palette.setColor(QPalette::Highlight, QColor(64, 64, 64)); //TODO: #GDM skin colors
    } else if (!index.data(QnBusiness::ValidRole).toBool()) {
        QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
        option->palette.setColor(QPalette::Highlight, clr.lighter()); //TODO: #GDM skin colors
        //option->palette.setColor(QPalette::Highlight, QColor(127, 0, 0, 127)); //TODO: #GDM skin colors
    } /*else
        option->palette.setColor(QPalette::Highlight, QColor(127, 127, 127, 255));*/
}

void QnBusinessRuleItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    if (QnIndexedDialogButton* btn = dynamic_cast<QnIndexedDialogButton *>(editor)){
        int role = index.column() == QnBusiness::SourceColumn
                ? QnBusiness::EventResourcesRole
                : QnBusiness::ActionResourcesRole;

        btn->setResources(index.data(role).value<QnResourceList>());
        return;
    }

    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    if (QnIndexedDialogButton* btn = dynamic_cast<QnIndexedDialogButton *>(editor)){
        model->setData(index, QVariant::fromValue<QnResourceList>(btn->resources()));
        return;
    }

    base_type::setModelData(editor, model, index);
}


