#include "business_rule_item_delegate.h"

#include <QtGui/QLabel>
#include <QtGui/QLayout>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/models/business_rules_view_model.h>
#include <ui/style/globals.h>

namespace {

    class QnRecordingEnabledDelegate: public QnSelectCamerasDialogDelegate {

    public:
        QnRecordingEnabledDelegate(QWidget* parent):
            QnSelectCamerasDialogDelegate(parent),
            m_recordingLabel(new QLabel(parent))
        {
            QPalette palette = parent->palette();
            palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
            m_recordingLabel->setPalette(palette);
        }

        virtual void setWidgetLayout(QLayout *layout) override {
            layout->addWidget(m_recordingLabel);
        }

        virtual void modelDataChanged(const QnResourceList &selected) override {
            int disabled = 0;
            QnVirtualCameraResourceList cameras = selected.filtered<QnVirtualCameraResource>();
            foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
                if (camera->isScheduleDisabled()) {
                    disabled++;
                }
            }
            m_recordingLabel->setText(tr("Recording is disabled for %1 of %2 selected cameras")
                                      .arg(disabled)
                                      .arg(cameras.size()));
            m_recordingLabel->setVisible(disabled > 0);
        }
    private:
        QLabel* m_recordingLabel;

    };

}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectResourcesDialogButton ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget *parent):
    base_type(parent),
    m_dialogDelegate(NULL)
{
    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QnResourceList QnSelectResourcesDialogButton::resources() {
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(QnResourceList resources) {
    m_resources = resources;
}

QnSelectCamerasDialogDelegate* QnSelectResourcesDialogButton::dialogDelegate() {
    return m_dialogDelegate;
}

void QnSelectResourcesDialogButton::setDialogDelegate(QnSelectCamerasDialogDelegate* delegate) {
    m_dialogDelegate = delegate;
}

void QnSelectResourcesDialogButton::at_clicked() {
    QnSelectCamerasDialog dialog(this); //TODO: #GDM servers dialog?
    dialog.setSelectedResources(m_resources);
    dialog.setDelegate(m_dialogDelegate);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_resources = dialog.getSelectedResources();
}

void QnSelectResourcesDialogButton::initStyleOption(QStyleOptionButton *option) const {
    base_type::initStyleOption(option);
}

void QnSelectResourcesDialogButton::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);
}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnBusinessRuleItemDelegate ---------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////


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
        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
//        btn->setFlat(true);
        btn->setText(tr("Select cameras...")); //TODO: target type
        return btn;
    }
    if (index.column() == QnBusiness::TargetColumn) {
        QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
//        btn->setFlat(true);
        btn->setText(tr("Select cameras...")); //TODO: target type
        if (index.data(QnBusiness::ActionTypeRole).toInt() == BusinessActionType::BA_CameraRecording)
            btn->setDialogDelegate(new QnRecordingEnabledDelegate(parent));
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
    if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
        int role = index.column() == QnBusiness::SourceColumn
                ? QnBusiness::EventResourcesRole
                : QnBusiness::ActionResourcesRole;

        btn->setResources(index.data(role).value<QnResourceList>());
        return;
    }

    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor)){
        model->setData(index, QVariant::fromValue<QnResourceList>(btn->resources()));
        return;
    }

    base_type::setModelData(editor, model, index);
}


