#include "interactive_settings_test_dialog.h"
#include "ui_interactive_settings_test_dialog.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/log/assert.h>
#include <client_core/client_core_module.h>

namespace nx::vms::client::desktop {

InteractiveSettingsTestDialog::InteractiveSettingsTestDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::InteractiveSettingsTestDialog()),
    m_settingsWidget(new QQuickWidget(qnClientCoreModule->mainQmlEngine(), this))
{
    ui->setupUi(this);
    ui->valuesTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->settingsTab->layout()->addWidget(m_settingsWidget);

    m_settingsWidget->setClearColor(palette().window().color());
    m_settingsWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_settingsWidget->setSource(QUrl("Nx/InteractiveSettings/SettingsView.qml"));

    if (!NX_ASSERT(m_settingsWidget->rootObject()))
        return;

    connect(m_settingsWidget->rootObject(), SIGNAL(valuesChanged()), this, SLOT(refreshValues()));

    connect(ui->loadButton, &QPushButton::clicked, this,
        &InteractiveSettingsTestDialog::loadManifest);

    loadManifest();
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
            Q_ARG(QVariant, model.toVariantMap()),
            Q_ARG(QVariant, {}));

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

        const QVariantMap values = result.toMap();

        ui->valuesTableWidget->setRowCount(values.size());

        int i = 0;
        for (auto it = values.begin(); it != values.end(); ++it, ++i)
        {
            ui->valuesTableWidget->setItem(i, 0, new QTableWidgetItem(it.key()));
            ui->valuesTableWidget->setItem(i, 1, new QTableWidgetItem(it.value().toString()));
        }
    }
}

InteractiveSettingsTestDialog::~InteractiveSettingsTestDialog() = default;

} // namespace nx::vms::client::desktop
