// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_dialog.h"

#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "../utils/debug_custom_actions.h"
#include "edit_property_dialog.h"

namespace nx::vms::client::desktop {

namespace {

std::unique_ptr<QFile> historyFile(QFile::OpenMode mode)
{
    static const QString kFileName = "nx_qml_test_dialog_history.txt";

    const auto location = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    auto file = std::make_unique<QFile>(QDir(location).filePath(kFileName));
    if (file->open(mode | QFile::Text))
        return std::move(file);

    return {};
}

} // namespace

struct QmlTestDialog::Private
{
    QmlTestDialog* const q;

    QWidget* const header = new QWidget(q);
    QQuickWidget* const qmlWidget = new QQuickWidget(appContext()->qmlEngine(), q);
    QDialogButtonBox* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, q);

    QLabel* const label = new QLabel("Component path:", header);
    QComboBox* const editBox = new QComboBox(header);
    QPushButton* const loadButton = new QPushButton("Load", header);
    QPushButton* const setPropertyButton = new QPushButton("Set Property...", header);

    void loadHistory()
    {
        const auto history = historyFile(QFile::ReadOnly);
        if (!history)
            return;

        QTextStream in(history.get());
        in.setEncoding(QStringConverter::Utf8);

        while (!in.atEnd())
        {
            const auto line = in.readLine().trimmed();
            if (!line.isEmpty())
                editBox->addItem(line);
        }
    }

    void saveHistory()
    {
        const auto history = historyFile(QFile::WriteOnly | QFile::Truncate);
        if (!history)
            return;

        QTextStream out(history.get());
        out.setEncoding(QStringConverter::Utf8);

        for (int i = 0; i < editBox->count(); ++i)
            out << editBox->itemText(i) << "\n";
    }
};

QmlTestDialog::QmlTestDialog(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    setWindowTitle("QML Component Test");
    resize(800, 600);

    setButtonBox(d->buttonBox);

    const auto contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, 0, style::Metrics::kDefaultTopLevelMargin, 0);

    const auto layout = new QVBoxLayout(this);
    layout->addWidget(d->header);
    layout->addLayout(contentLayout);
    layout->addWidget(d->buttonBox);

    const auto frame = new QFrame(this);
    frame->setFrameStyle(QFrame::Box);
    contentLayout->addWidget(frame);

    const auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins({});
    frameLayout->addWidget(d->qmlWidget);

    d->editBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    d->editBox->setEditable(true);
    d->editBox->setInsertPolicy(QComboBox::InsertAtTop);
    d->editBox->setMaxCount(16);
    d->editBox->completer()->setCaseSensitivity(Qt::CaseSensitive);
    d->loadButton->setEnabled(false);
    d->setPropertyButton->setEnabled(false);

    const auto headerLayout = new QHBoxLayout(d->header);
    headerLayout->addWidget(d->label);
    headerLayout->addWidget(d->editBox);
    headerLayout->addWidget(d->loadButton);
    headerLayout->addWidget(d->setPropertyButton);

    connect(d->loadButton, &QPushButton::clicked, this,
        [this]()
        {
            d->qmlWidget->setSource({});
            d->qmlWidget->engine()->clearComponentCache();
            d->qmlWidget->setSource(QUrl::fromUserInput(d->editBox->currentText().trimmed()));
            d->saveHistory();
        });

    connect(d->editBox, &QComboBox::currentTextChanged, this,
        [this](const QString& text) { d->loadButton->setEnabled(!text.trimmed().isEmpty()); });

    connect(d->editBox, qOverload<int>(&QComboBox::activated), this,
        [this](int index)
        {
            const auto text = d->editBox->itemText(index);
            d->editBox->removeItem(index);
            d->editBox->insertItem(0, text);
            d->editBox->setCurrentText(text);
            d->loadButton->click();
        });

    connect(d->editBox->lineEdit(), &QLineEdit::returnPressed, d->loadButton, &QPushButton::click);

    connect(d->setPropertyButton, &QPushButton::clicked, this,
        [this]()
        {
            EditPropertyDialog dialog;
            if (dialog.exec() == QDialog::Rejected)
                return;

            if (!d->qmlWidget->rootObject()->setProperty(
                dialog.propertyName().toLatin1(), dialog.propertyValue()))
            {
                QnMessageBox::critical(this, "Failed to set property value");
            }
        });

    connect(d->qmlWidget, &QQuickWidget::statusChanged, this,
        [this](QQuickWidget::Status status)
        {
            d->setPropertyButton->setEnabled(status == QQuickWidget::Status::Ready);

            if (status != QQuickWidget::Status::Error)
                return;

            QString message;
            for (const auto& error: d->qmlWidget->errors())
                message += error.toString() + "\n";

            QnMessageBox::critical(this, "Error loading QML component", message);
        });

    installEventHandler(this, QEvent::Show, this, [this]() { d->editBox->setFocus(); });

    d->qmlWidget->setClearColor(palette().window().color());
    d->qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    d->qmlWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d->loadHistory();
}

QmlTestDialog::~QmlTestDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}

void QmlTestDialog::registerAction()
{
    registerDebugAction(
        "QML Components",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<QmlTestDialog>(context->mainWindowWidget());
            dialog->exec();
        });

    registerDebugAction(
        "QML Dialog",
        [](QnWorkbenchContext* context)
        {
            static bool sRegistered = false;
            if (!sRegistered)
            {
                qmlRegisterType<QmlDialogWrapper>(
                    "nx.vms.client.desktop.debug", 1, 0, "QmlDialogWrapper");
                sRegistered = true;
            }

            const auto wrapper = new QmlDialogWrapper(
                QUrl("Nx/Dialogs/Test/DialogWrapperTestDialog.qml"),
                /*initialProperties*/ {},
                context->mainWindowWidget());
            connect(wrapper, &QmlDialogWrapper::done, wrapper, &QObject::deleteLater);
            wrapper->exec();
        });
}

} // namespace nx::vms::client::desktop
