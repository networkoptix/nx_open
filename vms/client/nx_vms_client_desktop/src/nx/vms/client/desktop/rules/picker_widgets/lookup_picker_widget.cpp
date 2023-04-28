// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_picker_widget.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <api/common_message_processor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/field_types.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>

namespace nx::vms::client::desktop::rules {

using LookupCheckType = vms::rules::LookupCheckType;
using LookupSource = vms::rules::LookupSource;

LookupPicker::LookupPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
    FieldPickerWidget<vms::rules::LookupField>(context, parent)
{
    const auto contentLayout = new QVBoxLayout;

    const auto typeLayout = new QHBoxLayout;

    m_checkTypeComboBox = new QComboBox;
    m_checkTypeComboBox->addItem(tr("In"), QVariant::fromValue(LookupCheckType::in));
    m_checkTypeComboBox->addItem(tr("Out"), QVariant::fromValue(LookupCheckType::out));

    typeLayout->addWidget(m_checkTypeComboBox);

    m_checkSourceComboBox = new QComboBox;
    m_checkSourceComboBox->addItem(tr("Keywords"), QVariant::fromValue(LookupSource::keywords));
    m_checkSourceComboBox->addItem(tr("Lookup List"), QVariant::fromValue(LookupSource::lookupList));

    typeLayout->addWidget(m_checkSourceComboBox);

    contentLayout->addLayout(typeLayout);

    m_stackedWidget = new QStackedWidget;

    m_lineEdit = new QLineEdit;
    m_lineEdit->setPlaceholderText(tr("Keywords separated by space"));
    m_lookupListComboBox = new QComboBox;
    for (const auto& list: systemContext()->lookupListManager()->lookupLists())
        m_lookupListComboBox->addItem(list.name, QVariant::fromValue(list.id));

    m_stackedWidget->addWidget(m_lineEdit);
    m_stackedWidget->addWidget(m_lookupListComboBox);

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
        m_stackedWidget->setCurrentWidget(m_lineEdit);
        m_lineEdit->setText(theField()->value());
    }
    else
    {
        m_stackedWidget->setCurrentWidget(m_lookupListComboBox);
        m_lookupListComboBox->setCurrentIndex(
            m_lookupListComboBox->findData(QVariant::fromValue(QnUuid{theField()->value()})));
    }
}

} // namespace nx::vms::client::desktop::rules
