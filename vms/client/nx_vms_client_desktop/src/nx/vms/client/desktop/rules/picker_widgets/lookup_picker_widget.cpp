// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_picker_widget.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <api/common_message_processor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/field_types.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>
#include <ui/widgets/common/elided_label.h>

namespace nx::vms::client::desktop::rules {

using LookupCheckType = vms::rules::LookupCheckType;
using LookupSource = vms::rules::LookupSource;

LookupPicker::LookupPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
    TitledFieldPickerWidget<vms::rules::LookupField>(context, parent)
{
    setCheckBoxEnabled(false);

    const auto contentLayout = new QVBoxLayout;

    const auto typeLayout = new QHBoxLayout;
    typeLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

    typeLayout->addWidget(new QWidget);

    auto comboBoxesLayout = new QHBoxLayout;

    m_checkTypeComboBox = new QComboBox;
    m_checkTypeComboBox->addItem(tr("Contains"), QVariant::fromValue(LookupCheckType::in));
    m_checkTypeComboBox->addItem(tr("Does not Contain"), QVariant::fromValue(LookupCheckType::out));

    comboBoxesLayout->addWidget(m_checkTypeComboBox);

    m_checkSourceComboBox = new QComboBox;
    m_checkSourceComboBox->addItem(tr("Keywords"), QVariant::fromValue(LookupSource::keywords));
    m_checkSourceComboBox->addItem(tr("List Entries"), QVariant::fromValue(LookupSource::lookupList));

    comboBoxesLayout->addWidget(m_checkSourceComboBox);

    typeLayout->addLayout(comboBoxesLayout);

    typeLayout->setStretch(0, 1);
    typeLayout->setStretch(1, 5);

    contentLayout->addLayout(typeLayout);

    m_stackedWidget = new QStackedWidget;

    {
        auto keywordsWidget = new QWidget;
        auto keywordsLayout = new QHBoxLayout{keywordsWidget};

        keywordsLayout->addWidget(new QWidget);

        m_lineEdit = new QLineEdit;
        m_lineEdit->setPlaceholderText(tr("Keywords separated by space"));
        keywordsLayout->addWidget(m_lineEdit);

        keywordsLayout->setStretch(0, 1);
        keywordsLayout->setStretch(1, 5);

        m_stackedWidget->addWidget(keywordsWidget);
    }

    {
        auto lookupListsWidget = new QWidget;
        auto lookupListsLayout = new QHBoxLayout{lookupListsWidget};

        auto lookupListsLabel = new QnElidedLabel;
        lookupListsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lookupListsLabel->setElideMode(Qt::ElideRight);
        lookupListsLabel->setText(tr("From"));
        lookupListsLayout->addWidget(lookupListsLabel);

        m_lookupListComboBox = new QComboBox;
        for (const auto& list: systemContext()->lookupListManager()->lookupLists())
            m_lookupListComboBox->addItem(list.name, QVariant::fromValue(list.id));

        lookupListsLayout->addWidget(m_lookupListComboBox);

        lookupListsLayout->setStretch(0, 1);
        lookupListsLayout->setStretch(1, 5);

        m_stackedWidget->addWidget(lookupListsWidget);
    }

    contentLayout->addWidget(m_stackedWidget);

    m_contentWidget->setLayout(contentLayout);

    connect(
        m_checkTypeComboBox,
        &QComboBox::activated,
        this,
        [this]
        {
            theField()->setCheckType(m_checkTypeComboBox->currentData().value<LookupCheckType>());
        });

    connect(
        m_checkSourceComboBox,
        &QComboBox::activated,
        this,
        [this]
        {
            theField()->setSource(m_checkSourceComboBox->currentData().value<LookupSource>());
            theField()->setValue("");
        });

    const auto lookupListNotificationManager =
        systemContext()->messageProcessor()->connection()->lookupListNotificationManager().get();
    connect(
        lookupListNotificationManager,
        &ec2::AbstractLookupListNotificationManager::addedOrUpdated,
        this,
        [this](const nx::vms::api::LookupListData& data)
        {
            if (m_lookupListComboBox->findData(QVariant::fromValue(data.id)) != -1)
                m_lookupListComboBox->addItem(data.name, QVariant::fromValue(data.id));
        });

    connect(
        lookupListNotificationManager,
        &ec2::AbstractLookupListNotificationManager::removed,
        this,
        [this](QnUuid id)
        {
            m_lookupListComboBox->removeItem(m_lookupListComboBox->findData(QVariant::fromValue(id)));
        });

    connect(
        m_lookupListComboBox,
        &QComboBox::activated,
        this,
        [this](int index)
        {
            theField()->setValue(m_lookupListComboBox->itemData(index).value<QnUuid>().toString());
        });

    connect(
        m_lineEdit,
        &QLineEdit::textEdited,
        this,
        [this](const QString& text)
        {
            theField()->setValue(text);
        });
}

void LookupPicker::updateUi()
{
    m_checkTypeComboBox->setCurrentIndex(
        m_checkTypeComboBox->findData(QVariant::fromValue(theField()->checkType())));
    m_checkSourceComboBox->setCurrentIndex(
        m_checkSourceComboBox->findData(QVariant::fromValue(theField()->source())));
    if (theField()->source() == LookupSource::keywords)
    {
        m_stackedWidget->setCurrentIndex(0);
        m_lineEdit->setText(theField()->value());
    }
    else
    {
        m_stackedWidget->setCurrentIndex(1);
        m_lookupListComboBox->setCurrentIndex(
            m_lookupListComboBox->findData(QVariant::fromValue(QnUuid{theField()->value()})));
    }
}

} // namespace nx::vms::client::desktop::rules
