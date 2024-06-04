// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_lookup_picker_widget.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/field_types.h>
#include <ui/widgets/common/elided_label.h>

#include "../model_view/lookup_lists_model.h"

namespace nx::vms::client::desktop::rules {

using LookupCheckType = vms::rules::TextLookupCheckType;

TextLookupPicker::TextLookupPicker(
    vms::rules::TextLookupField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    TitledFieldPickerWidget<vms::rules::TextLookupField>(field, context, parent)
{
    setCheckBoxEnabled(false);

    const auto contentLayout = new QVBoxLayout;

    const auto typeLayout = new QHBoxLayout;
    typeLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

    typeLayout->addWidget(new QWidget);

    auto comboBoxesLayout = new QHBoxLayout;

    m_checkTypeComboBox = new QComboBox;
    m_checkTypeComboBox->addItem(
        tr("Contains keywords"),
        QVariant::fromValue(LookupCheckType::containsKeywords));
    m_checkTypeComboBox->addItem(
        tr("Does not contain keywords"),
        QVariant::fromValue(LookupCheckType::doesNotContainKeywords));
    m_checkTypeComboBox->addItem(
        tr("Contains list entries"),
        QVariant::fromValue(LookupCheckType::inList));
    m_checkTypeComboBox->addItem(
        tr("Does not contain list entries"),
        QVariant::fromValue(LookupCheckType::notInList));

    comboBoxesLayout->addWidget(m_checkTypeComboBox);

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

        auto lookupListsModel = new LookupListsModel{context, this};
        lookupListsModel->setObjectTypeId("");
        auto sortModel = new QSortFilterProxyModel{this};
        sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        sortModel->setSourceModel(lookupListsModel);
        sortModel->sort(0);

        m_lookupListComboBox->setModel(sortModel);
        m_lookupListComboBox->setPlaceholderText(tr("Select a list"));

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
            m_field->setCheckType(m_checkTypeComboBox->currentData().value<LookupCheckType>());
        });

    connect(
        m_lookupListComboBox,
        &QComboBox::activated,
        this,
        [this](int index)
        {
            m_field->setValue(m_lookupListComboBox->itemData(index).value<nx::Uuid>().toString());
        });

    connect(
        m_lineEdit,
        &QLineEdit::textEdited,
        this,
        [this](const QString& text)
        {
            m_field->setValue(text);
        });
}

void TextLookupPicker::updateUi()
{
    TitledFieldPickerWidget<vms::rules::TextLookupField>::updateUi();

    m_checkTypeComboBox->setCurrentIndex(
        m_checkTypeComboBox->findData(QVariant::fromValue(m_field->checkType())));
    switch (m_field->checkType())
    {
        case LookupCheckType::containsKeywords:
        case LookupCheckType::doesNotContainKeywords:
            if (nx::Uuid::isUuidString(m_field->value()))
                m_field->setValue({});

            m_stackedWidget->setCurrentIndex(0);
            m_lineEdit->setText(m_field->value());
            return;
        case LookupCheckType::inList:
        case LookupCheckType::notInList:
            if (!nx::Uuid::isUuidString(m_field->value()))
                m_field->setValue({});

            const auto matches = m_lookupListComboBox->model()->match(
                m_lookupListComboBox->model()->index(0, 0),
                LookupListsModel::LookupListIdRole,
                QVariant::fromValue(nx::Uuid{m_field->value()}),
                /*hits*/ 1,
                Qt::MatchExactly);

            m_lookupListComboBox->setCurrentIndex(matches.size() == 1 ? matches[0].row() : -1);

            m_stackedWidget->setCurrentIndex(1);
            return;
    }
}

} // namespace nx::vms::client::desktop::rules
