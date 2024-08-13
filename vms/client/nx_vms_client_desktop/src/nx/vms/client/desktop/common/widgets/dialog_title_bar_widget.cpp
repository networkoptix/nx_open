// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dialog_title_bar_widget.h"
#include "ui_dialog_title_bar_widget.h"

#include <QtCore/QPointer>
#include <QtWidgets/QWhatsThis>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kCaptionFontPixelSize = 14;
static constexpr auto kCaptionFontWeight = QFont::Bold;

} // namespace

// ------------------------------------------------------------------------------------------------
// DialogTitleBarWidget::Private

class DialogTitleBarWidget::Private: public QObject
{
    DialogTitleBarWidget* const q;
    const QScopedPointer<Ui::DialogTitleBarWidget> ui;

    using base_type = QObject;

public:
    Private(DialogTitleBarWidget* q):
        q(q),
        ui(new Ui::DialogTitleBarWidget())
    {
        ui->setupUi(q);
        ui->captionLabel->setForegroundRole(QPalette::Text);
        ui->captionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

        QFont font;
        font.setPixelSize(kCaptionFontPixelSize);
        font.setWeight(kCaptionFontWeight);
        ui->captionLabel->setFont(font);

        ui->helpButton->setIcon(qnSkin->icon("titlebar/window_question.svg"));
        ui->minimizeButton->setIcon(qnSkin->icon("titlebar/window_minimize.svg"));
        const QColor background = core::colorTheme()->color("dark7");
        const core::SvgIconColorer::IconSubstitutions colorSubs = {
            { QnIcon::Active, {{ background, "red" }}},
            { QnIcon::Pressed, {{ background, "red" }}}};
        ui->closeButton->setIcon(qnSkin->icon("titlebar/window_close.svg", nullptr, nullptr, colorSubs));

        ui->helpButton->setFixedSize(qnSkin->maximumSize(ui->helpButton->icon()));
        ui->minimizeButton->setFixedSize(qnSkin->maximumSize(ui->minimizeButton->icon()));
        ui->closeButton->setFixedSize(qnSkin->maximumSize(ui->closeButton->icon()));

        connect(ui->helpButton, &QPushButton::clicked, this,
            []
            {
                if (QWhatsThis::inWhatsThisMode())
                    QWhatsThis::leaveWhatsThisMode();
                else
                    QWhatsThis::enterWhatsThisMode();
            });

        connect(ui->minimizeButton, &QPushButton::clicked, this,
            [this]()
            {
                if (m_window)
                    m_window->showMinimized();
            });

        connect(ui->maximizeButton, &QPushButton::clicked, this,
            [this]()
            {
                if (!m_window)
                    return;

                if (m_window->isMaximized() || m_window->isFullScreen())
                    m_window->showNormal();
                else
                    m_window->showMaximized();
            });

        connect(ui->closeButton, &QPushButton::clicked, this,
            [this]()
            {
                if (m_window)
                    m_window->close();
            });

        q->setFixedHeight(ui->closeButton->maximumHeight());
        updateWindow();
    }

    void updateWindow()
    {
        if (m_window == q->window())
            return;

        if (m_window)
            m_window->removeEventFilter(this);

        m_window = q->window();

        if (m_window)
            m_window->installEventFilter(this);

        updateAll();
    }

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_window)
            return base_type::eventFilter(watched, event);

        switch (event->type())
        {
            case QEvent::WindowTitleChange:
                updateCaption();
                break;

            case QEvent::Show:
            case QEvent::Hide:
                updateButtons();
                break;

            case QEvent::WindowStateChange:
                updateState();
                break;

            case QEvent::WindowIconChange:
                updateIcon();
                break;

            default:
                break;
        }

        return base_type::eventFilter(watched, event);
    }

private:
    void updateAll()
    {
        updateIcon();
        updateCaption();
        updateButtons();
        updateState();
    }

    void updateCaption()
    {
        ui->captionLabel->setText(m_window
            ? m_window->windowTitle()
            : QString());
    }

    void updateIcon()
    {
        ui->iconLabel->setPixmap(m_window
            ? m_window->windowIcon().pixmap(q->maximumHeight())
            : QPixmap());

        ui->iconLabel->setVisible(!ui->iconLabel->pixmap().isNull());
    }

    void updateState()
    {
        ui->maximizeButton->setIcon(m_window && (m_window->isMaximized() || m_window->isFullScreen())
            ? qnSkin->icon("titlebar/window_restore.svg")
            : qnSkin->icon("titlebar/window_maximize.svg"));

        ui->maximizeButton->setFixedSize(qnSkin->maximumSize(ui->maximizeButton->icon()));
    }

    void updateButtons()
    {
        if (m_window)
        {
            const auto flags = m_window->windowFlags();
            ui->helpButton->setVisible(flags.testFlag(Qt::WindowContextHelpButtonHint));
            ui->closeButton->setVisible(flags.testFlag(Qt::WindowCloseButtonHint));
            ui->minimizeButton->setVisible(flags.testFlag(Qt::WindowMinimizeButtonHint));
            ui->maximizeButton->setVisible(flags.testFlag(Qt::WindowMaximizeButtonHint));
        }
        else
        {
            ui->helpButton->hide();
            ui->maximizeButton->hide();
            ui->maximizeButton->hide();
            ui->closeButton->hide();
        }
    }

private:
    QPointer<QWidget> m_window;
};

// ------------------------------------------------------------------------------------------------
// DialogTitleBarWidget

DialogTitleBarWidget::DialogTitleBarWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Text, core::colorTheme()->color("light4"));
}

DialogTitleBarWidget::~DialogTitleBarWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void DialogTitleBarWidget::paintEvent(QPaintEvent* event)
{
    d->updateWindow(); //< Check if the widget was reparented into another window.
    base_type::paintEvent(event);
}

} // namespace nx::vms::client::desktop
