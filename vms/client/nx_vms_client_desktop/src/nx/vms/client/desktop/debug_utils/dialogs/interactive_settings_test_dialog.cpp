// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "interactive_settings_test_dialog.h"
#include "ui_interactive_settings_test_dialog.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

InteractiveSettingsTestDialog::InteractiveSettingsTestDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::InteractiveSettingsTestDialog()),
    m_settingsWidget(new QQuickWidget(appContext()->qmlEngine(), this))
{
    ui->setupUi(this);
    ui->valuesTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->settingsTab->layout()->addWidget(m_settingsWidget);

    setMonospaceFont(ui->manifestTextEdit);

    m_settingsWidget->setClearColor(palette().window().color());
    m_settingsWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_settingsWidget->setSource(QUrl("Nx/InteractiveSettings/SettingsView.qml"));

    if (!NX_ASSERT(m_settingsWidget->rootObject()))
        return;

    connect(m_settingsWidget->rootObject(), SIGNAL(valuesChanged()), this, SLOT(refreshValues()));

    connect(ui->loadButton, &QPushButton::clicked, this,
        &InteractiveSettingsTestDialog::loadManifest);

    QFile demoFile(":/test_data/interactive_settings_demo.json");
    if (demoFile.open(QFile::ReadOnly))
        ui->manifestTextEdit->setPlainText(QString::fromUtf8(demoFile.readAll()));

    loadManifest();
}

InteractiveSettingsTestDialog::~InteractiveSettingsTestDialog()
{
}

void InteractiveSettingsTestDialog::loadManifest()
{
    const QString serializedModel = ui->manifestTextEdit->toPlainText().trimmed();
    const QJsonObject model = QJsonDocument::fromJson(serializedModel.toUtf8()).object();

    if (const auto rootObject = m_settingsWidget->rootObject())
    {
        QMetaObject::invokeMethod(
            m_settingsWidget->rootObject(),
            "loadModel",
            Qt::DirectConnection,
            Q_ARG(QVariant, model),
            /*initialValues*/ Q_ARG(QVariant, {}),
            /*sectionPath*/ Q_ARG(QVariant, {}),
            /*restoreScrollPosition*/ Q_ARG(QVariant, false));

        refreshValues();
    }
}

void InteractiveSettingsTestDialog::refreshValues()
{
    ui->valuesTableWidget->clear();

    if (const auto rootObject = m_settingsWidget->rootObject())
    {
        QVariant result;

        QMetaObject::invokeMethod(rootObject, "getValues", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, result));

        const auto& values = QJsonObject::fromVariantMap(result.toMap());

        ui->valuesTableWidget->setRowCount(values.size());

        int i = 0;
        for (auto it = values.begin(); it != values.end(); ++it, ++i)
        {
            ui->valuesTableWidget->setItem(i, 0, new QTableWidgetItem(it.key()));
            ui->valuesTableWidget->setItem(i, 1, new QTableWidgetItem(
                QString::fromUtf8(QJson::serialize(it.value()))));
        }
    }
}

void InteractiveSettingsTestDialog::registerAction()
{
    registerDebugAction(
        "Interactive Settings",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<InteractiveSettingsTestDialog>(
                context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
