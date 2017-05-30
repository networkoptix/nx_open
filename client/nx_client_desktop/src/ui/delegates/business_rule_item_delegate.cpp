#include "business_rule_item_delegate.h"

#include <QtGui/QPainter>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>

#include <business/business_action_parameters.h>
#include <business/business_strings_helper.h>
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>

#include <client_core/client_core_module.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_criterion.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/models/business_rules_view_model.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/math/color_transformations.h>

namespace {
enum { comboBoxMaxVisibleItems = 100 };
}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnSelectResourcesDialogButton ------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget *parent):
    base_type(parent),
    m_dialogDelegate(NULL),
    m_target(QnResourceSelectionDialog::Filter::cameras)
{
    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QSet<QnUuid> QnSelectResourcesDialogButton::resources() const
{
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(QSet<QnUuid> resources)
{
    m_resources = resources;
}

QnResourceSelectionDialogDelegate* QnSelectResourcesDialogButton::dialogDelegate() const
{
    return m_dialogDelegate;
}

void QnSelectResourcesDialogButton::setDialogDelegate(QnResourceSelectionDialogDelegate* delegate)
{
    m_dialogDelegate = delegate;
}

QnResourceSelectionDialog::Filter  QnSelectResourcesDialogButton::selectionTarget() const
{
    return m_target;
}

void QnSelectResourcesDialogButton::setSelectionTarget(QnResourceSelectionDialog::Filter target)
{
    m_target = target;
}

void QnSelectResourcesDialogButton::at_clicked()
{
    QnResourceSelectionDialog dialog(m_target, this);
    dialog.setSelectedResources(m_resources);
    dialog.setDelegate(m_dialogDelegate);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
        return;
    m_resources = dialog.selectedResources();
    emit commit();
}

void QnSelectResourcesDialogButton::initStyleOption(QStyleOptionButton *option) const
{
    base_type::initStyleOption(option);
}

void QnSelectResourcesDialogButton::paintEvent(QPaintEvent *event)
{
    base_type::paintEvent(event);
}

///////////////////////////////////////////////////////////////////////////////////////
//---------------- QnBusinessRuleItemDelegate ---------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////
QnBusinessRuleItemDelegate::QnBusinessRuleItemDelegate(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_lexComparator(new QnBusinessTypesComparator()),
    m_businessStringsHelper(new QnBusinessStringsHelper(qnClientCoreModule->commonModule()))
{
}

QnBusinessRuleItemDelegate::~QnBusinessRuleItemDelegate()
{
}

int QnBusinessRuleItemDelegate::optimalWidth(int column, const QFontMetrics& metrics)
{
    const int kExtraSpace =
        style::Metrics::kStandardPadding //< dropdown text indent
        + style::Metrics::kButtonHeight; //< dropdown arrow

    switch (column)
    {
        case QnBusiness::EventColumn:
        {
            QnBusinessStringsHelper helper(qnClientCoreModule->commonModule());
            auto eventWidth = [&metrics, &helper](QnBusiness::EventType eventType)
                {
                    return metrics.width(helper.eventName(eventType));
                };
            int result = -1;
            for (QnBusiness::EventType eventType: QnBusiness::allEvents())
                result = qMax(result, eventWidth(eventType));
            return kExtraSpace + result;
        }
        case QnBusiness::ActionColumn:
        {
            QnBusinessStringsHelper helper(qnClientCoreModule->commonModule());
            auto actionWidth = [&metrics, &helper](QnBusiness::ActionType actionType)
                {
                    return metrics.width(helper.actionName(actionType));
                };
            int result = -1;
            for (QnBusiness::ActionType actionType: QnBusiness::allActions())
                result = qMax(result, actionWidth(actionType));
            return kExtraSpace + result;
        }
        case QnBusiness::AggregationColumn:
        {
            return QnAggregationWidget::optimalWidth();
        }
        default:
            break;
    }
    return -1;
}

void QnBusinessRuleItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (index.data(Qn::DisabledRole).toBool())
    {
        if (auto vopt = qstyleoption_cast<QStyleOptionViewItemV4*>(option))
            vopt->state &= ~QStyle::State_Enabled;

        option->palette.setColor(QPalette::Highlight,
            qnGlobals->businessRuleDisabledHighlightColor());
    }
    else if (!index.data(Qn::ValidRole).toBool())
    {
        static const auto kErrorHoverOpacity = 0.35;
        option->palette.setColor(QPalette::Midlight, toTransparent(
            option->palette.color(QPalette::Midlight), kErrorHoverOpacity));

        option->palette.setColor(QPalette::Highlight,
            qnGlobals->businessRuleInvalidHighlightColor());
    }
}

void QnBusinessRuleItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!option.state.testFlag(QStyle::State_Selected))
    {
        if (!index.data(Qn::DisabledRole).toBool() && !index.data(Qn::ValidRole).toBool())
            painter->fillRect(option.rect, qnGlobals->businessRuleInvalidBackgroundColor());
    }

    base_type::paint(painter, option, index);
}

QWidget* QnBusinessRuleItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    switch (index.column())
    {
        case QnBusiness::SourceColumn:
        {
            QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
            //TODO: #GDM #Business server selection dialog?
            connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

            QnBusiness::EventType eventType = index.data(Qn::EventTypeRole).value<QnBusiness::EventType>();
            if (eventType == QnBusiness::CameraMotionEvent)
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraMotionPolicy>(btn));
            else if (eventType == QnBusiness::CameraInputEvent)
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraInputPolicy>(btn));

            return btn;
        }
        case QnBusiness::TargetColumn:
        {
            QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

            switch (actionType)
            {
                case QnBusiness::ShowPopupAction:
                {
                    QComboBox* comboBox = new QComboBox(parent);
                    comboBox->addItem(tr("For Users"), QnBusiness::EveryOne);
                    comboBox->addItem(tr("For Administrators Only"), QnBusiness::AdminOnly);
                    return comboBox;
                }
                default:
                    break;
            }

            QnSelectResourcesDialogButton* btn = new QnSelectResourcesDialogButton(parent);
            connect(btn, SIGNAL(commit()), this, SLOT(at_editor_commit()));

            if (actionType == QnBusiness::CameraRecordingAction)
            {
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraRecordingPolicy>(btn));
            }
            else if (actionType == QnBusiness::BookmarkAction)
            {
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnBookmarkActionPolicy>(btn));
            }
            else if (actionType == QnBusiness::CameraOutputAction)
            {
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraOutputPolicy>(btn));
            }
            else if (actionType == QnBusiness::ExecutePtzPresetAction)
            {
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnExecPtzPresetPolicy>(btn));
            }
            else if (actionType == QnBusiness::SendMailAction)
            {
                btn->setDialogDelegate(new QnSendEmailActionDelegate(btn));
                btn->setSelectionTarget(QnResourceSelectionDialog::Filter::users);
            }
            else if (actionType == QnBusiness::PlaySoundAction ||
                actionType == QnBusiness::PlaySoundOnceAction ||
                actionType == QnBusiness::SayTextAction)
            {
                btn->setDialogDelegate(new QnCheckResourceAndWarnDelegate<QnCameraAudioTransmitPolicy>(btn));
            }
            return btn;
        }
        case QnBusiness::EventColumn:
        {
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);
            for (QnBusiness::EventType eventType : m_lexComparator->lexSortedEvents())
                comboBox->addItem(m_businessStringsHelper->eventName(eventType), eventType);
            return comboBox;
        }
        case QnBusiness::ActionColumn:
        {
            bool instantOnly = !QnBusiness::hasToggleState(index.data(Qn::EventTypeRole).value<QnBusiness::EventType>());
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);
            for (QnBusiness::ActionType actionType : m_lexComparator->lexSortedActions())
            {
                if (instantOnly && !QnBusiness::canBeInstant(actionType))
                    continue;
                comboBox->addItem(m_businessStringsHelper->actionName(actionType), actionType);
            }
            return comboBox;
        }
        case QnBusiness::AggregationColumn:
        {
            QnAggregationWidget* widget = new QnAggregationWidget(parent);
            widget->setShort(true);
            return widget;
        }
        default:
            break;
    }

    return base_type::createEditor(parent, option, index);
}


void QnBusinessRuleItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    switch (index.column())
    {
        case QnBusiness::SourceColumn:
        {
            if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor))
            {
                btn->setResources(index.data(Qn::EventResourcesRole).value<QSet<QnUuid>>());
                btn->setText(index.data(Qn::ShortTextRole).toString());
                return;
            }
            break;
        }
        case QnBusiness::TargetColumn:
        {
            QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

            switch (actionType)
            {
                case QnBusiness::ShowPopupAction:
                {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
                    {
                        comboBox->setCurrentIndex(comboBox->findData(index.data(Qt::EditRole)));
                        connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
                    }
                    return;
                }
                default:
                    break;
            }

            if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor))
            {
                btn->setResources(index.data(Qn::ActionResourcesRole).value<QSet<QnUuid>>());
                btn->setText(index.data(Qn::ShortTextRole).toString());
                return;
            }
            break;
        }
        case QnBusiness::EventColumn:
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
            {
                comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::EventTypeRole).value<QnBusiness::EventType>()));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        case QnBusiness::ActionColumn:
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
            {
                comboBox->setCurrentIndex(comboBox->findData(index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>()));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        case QnBusiness::AggregationColumn:
            if (QnAggregationWidget* widget = dynamic_cast<QnAggregationWidget *>(editor))
            {
                widget->setValue(index.data(Qt::EditRole).toInt());
                connect(widget, SIGNAL(valueChanged()), this, SLOT(at_editor_commit()));
            }
            return;
        default:
            break;
    }

    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    switch (index.column())
    {
        case QnBusiness::SourceColumn:
        {
            if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor))
            {
                model->setData(index, QVariant::fromValue<QSet<QnUuid>>(btn->resources()));
                return;
            }

            break;
        }
        case QnBusiness::TargetColumn:
        {
            QnBusiness::ActionType actionType = index.data(Qn::ActionTypeRole).value<QnBusiness::ActionType>();

            switch (actionType)
            {
                case QnBusiness::ShowPopupAction:
                {
                    if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
                    {
                        model->setData(index, comboBox->itemData(comboBox->currentIndex()));
                    }
                    return;
                }
                default:
                    break;
            }

            if (QnSelectResourcesDialogButton* btn = dynamic_cast<QnSelectResourcesDialogButton *>(editor))
            {
                model->setData(index, QVariant::fromValue<QSet<QnUuid>>(btn->resources()));
                return;
            }

            break;
        }
        case QnBusiness::EventColumn:
        case QnBusiness::ActionColumn:
            if (QComboBox* comboBox = dynamic_cast<QComboBox *>(editor))
            {
                model->setData(index, comboBox->itemData(comboBox->currentIndex()));
            }
            return;
        case QnBusiness::AggregationColumn:
            if (QnAggregationWidget* widget = dynamic_cast<QnAggregationWidget *>(editor))
            {
                model->setData(index, widget->value());
            }
            return;
        default:
            break;
    }
    base_type::setModelData(editor, model, index);
}

bool QnBusinessRuleItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    QComboBox *editor = qobject_cast<QComboBox*>(object);
    if (editor && event->type() == QEvent::FocusOut)
        return false;
    return base_type::eventFilter(object, event);
}

void QnBusinessRuleItemDelegate::at_editor_commit()
{
    if (QWidget* w = dynamic_cast<QWidget*> (sender()))
        emit commitData(w);
}

void QnBusinessRuleItemDelegate::updateEditorGeometry(QWidget* editor,
    const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    switch (index.column())
    {
        case QnBusiness::EventColumn:
        case QnBusiness::ActionColumn:
            editor->setGeometry(option.rect);
            break;

        default:
            base_type::updateEditorGeometry(editor, option, index);
    }
}
