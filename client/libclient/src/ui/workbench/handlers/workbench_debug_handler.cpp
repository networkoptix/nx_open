#include "workbench_debug_handler.h"

#include <QtWidgets/QLineEdit>
#include <QtWebKitWidgets/QWebView>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/widgets/common/web_page.h>
#include <ui/widgets/views/resource_list_view.h>

#include <utils/applauncher_utils.h>

//#ifdef _DEBUG
#define DEBUG_ACTIONS
//#endif

namespace {

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QnDialog, public QnWorkbenchContextAware
{
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent = NULL):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(newActionButton(QnActions::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugIncrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugShowResourcePoolAction));
        setLayout(layout);
    }

private:
    QToolButton *newActionButton(QnActions::IDType actionId, QWidget *parent = NULL)
    {
        QToolButton *button = new QToolButton(parent);
        button->setDefaultAction(menu()->action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }
};

class QnWebViewDialog: public QDialog
{
    using base_type = QDialog;
public:
    QnWebViewDialog(QWidget* parent = nullptr):
        base_type(parent, Qt::Window),
        m_page(new QnWebPage(this)),
        m_webView(new QWebView(this)),
        m_urlLineEdit(new QLineEdit(this))
    {
        m_webView->setPage(m_page);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->addWidget(m_urlLineEdit);
        layout->addWidget(m_webView);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, this, [this]()
        {
            m_webView->load(m_urlLineEdit->text());
        });
    }

    QString url() const { return m_urlLineEdit->text(); }
    void setUrl(const QString& value)
    {
        m_urlLineEdit->setText(value);
        m_webView->load(QUrl::fromUserInput(value));
    }

private:
    QnWebPage* m_page;
    QWebView* m_webView;
    QLineEdit* m_urlLineEdit;
};

class QnApplauncherControlDialog: public QDialog
{
    using base_type = QDialog;
public:
    QnApplauncherControlDialog(QWidget* parent = nullptr):
        base_type(parent, Qt::Window)
    {
        using namespace applauncher;

        auto l = new QVBoxLayout(this);

        {
            auto row = new QHBoxLayout();
            l->addLayout(row);

            auto button = new QPushButton(lit("Check version"), this);
            row->addWidget(button);

            auto edit = new QLineEdit(this);
            row->addWidget(edit, 1);

            auto result = new QLabel(this);
            row->addWidget(result);

            connect(button, &QPushButton::clicked, this, [edit, result]
            {
                QnSoftwareVersion v(edit->text());
                if (v.isNull())
                    v = qnCommon->engineVersion();

                bool isInstalled = false;
                auto errCode = isVersionInstalled(v, &isInstalled);

                result->setText(
                    lit("Version %1: %2 (%3)")
                        .arg(v.toString())
                        .arg(isInstalled ? lit("Installed") : lit("Not Installed"))
                        .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
                    );
            });
        }

        {
            auto row = new QHBoxLayout();
            l->addLayout(row);

            auto button = new QPushButton(lit("Get versions list"), this);
            row->addWidget(button);

            auto result = new QLabel(this);
            row->addWidget(result);

            connect(button, &QPushButton::clicked, this, [result]
            {
                QList<QnSoftwareVersion> versions;
                auto errCode = getInstalledVersions(&versions);

                QStringList text;
                for (auto v: versions)
                    text << v.toString();

                result->setText(
                    lit("Result %1:\n %2")
                    .arg(QString::fromUtf8(api::ResultType::toString(errCode)))
                    .arg(text.join(L'\n'))
                );
            });
        }

        l->addStretch();
        setMinimumHeight(500);
    }

};

}

// -------------------------------------------------------------------------- //
// QnWorkbenchDebugHandler
// -------------------------------------------------------------------------- //
QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
#ifdef DEBUG_ACTIONS
    //TODO: #GDM #High remove before release
    qDebug() << "------------- Debug actions ARE ACTIVE -------------";
    connect(action(QnActions::DebugControlPanelAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(QnActions::DebugIncrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(QnActions::DebugDecrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
    connect(action(QnActions::DebugShowResourcePoolAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered);
#endif
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered()
{
    QScopedPointer<QnDebugControlDialog> dialog(new QnDebugControlDialog(mainWindow()));
    dialog->exec();
}

void QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    qDebug() << qnRuntime->debugCounter();

    auto dialog(new QnApplauncherControlDialog(mainWindow()));
    dialog->show();
    return;

    auto showPalette = [this]
    {
        QnPaletteWidget *w = new QnPaletteWidget();
        w->setPalette(qApp->palette());
        w->show();
    };

    // the palette widget code might still be required
    Q_UNUSED(showPalette);
}

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered()
{
    auto dialog(new QnWebViewDialog(mainWindow()));
    dialog->setUrl(lit("http://localhost:7001"));
    dialog->show();
}

void QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered()
{
    QnMessageBox messageBox(mainWindow());
    messageBox.addCustomWidget(new QnResourceListView(qnResPool->getResources()));
    messageBox.exec();
}
