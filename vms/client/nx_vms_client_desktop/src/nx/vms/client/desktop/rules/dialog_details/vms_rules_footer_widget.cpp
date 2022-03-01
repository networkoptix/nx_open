// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_footer_widget.h"

#include "ui_vms_rules_footer_widget.h"

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {
namespace rules {

VmsRulesFooterWidget::VmsRulesFooterWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::VmsRulesFooterWidget())
{
    m_ui->setupUi(this);
    setupIconsAndColors();

    connect(m_ui->addCommentButton, &QPushButton::clicked, this,
        [this]
        {
            m_ui->stackedWidget->setCurrentWidget(m_ui->commentEditorPage);
            m_ui->commentLineEdit->setFocus(Qt::OtherFocusReason);
        });

    connect(m_ui->commitCommentButton, &QPushButton::clicked, this,
        [this] { m_ui->stackedWidget->setCurrentWidget(m_ui->mainPage); });

    installEventHandler({m_ui->commentLineEdit}, {QEvent::FocusOut}, this,
        [this] { m_ui->stackedWidget->setCurrentWidget(m_ui->mainPage); });

    connect(m_ui->commentLineEdit, &QLineEdit::textEdited, this,
        [this](const QString& text)
        {
            m_ui->commentDisplayLabel->setText(text.trimmed());
            if (!m_ui->commentDisplayLabel->text().isEmpty())
                m_ui->commentStackedWidget->setCurrentWidget(m_ui->commentDisplayPage);
            else
                m_ui->commentStackedWidget->setCurrentWidget(m_ui->commentButtonPage);
        });

    setupCommentLabelInteractions();
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
VmsRulesFooterWidget::~VmsRulesFooterWidget()
{
}

// Makes comment label clickable like a push button. Comment label is highlighted on mouse hover.
void VmsRulesFooterWidget::setupCommentLabelInteractions()
{
    installEventHandler(
        {m_ui->commentDisplayLabel},
        {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::HoverEnter, QEvent::HoverLeave},
        this,
        [this, wasPressed = false](QObject* /*object*/, QEvent* event) mutable
        {
            if (!m_ui->addCommentButton->isEnabled())
                return;

            if (event->type() == QEvent::MouseButtonPress)
                wasPressed = true;

            if (event->type() == QEvent::MouseButtonRelease && wasPressed)
            {
                wasPressed = false;
                const auto label = m_ui->commentDisplayLabel;
                if (label->geometry().contains(label->mapFromGlobal(QCursor::pos())))
                {
                    m_ui->stackedWidget->setCurrentWidget(m_ui->commentEditorPage);
                    m_ui->commentLineEdit->setFocus(Qt::OtherFocusReason);
                }
            }

            if (event->type() == QEvent::HoverEnter)
            {
                setPaletteColor(m_ui->commentDisplayLabel, QPalette::WindowText,
                    QPalette().color(QPalette::BrightText));
            }

            if (event->type() == QEvent::HoverLeave)
            {
                setPaletteColor(m_ui->commentDisplayLabel, QPalette::WindowText,
                    QPalette().color(QPalette::Light));
            }
        });
}

void VmsRulesFooterWidget::setupIconsAndColors()
{
    m_ui->enabledSwitch->setFixedSize(style::Metrics::kStandaloneSwitchSize);
    m_ui->setScheduleButton->setIcon(qnSkin->pixmap("text_buttons/calendar.png"));
    m_ui->setMaxFrequencyButton->setIcon(qnSkin->pixmap("text_buttons/timestamp.png"));
    m_ui->testActionButton->setIcon(qnSkin->pixmap("text_buttons/forward.png"));
    m_ui->commitCommentButton->setIcon(Skin::colorize(qnSkin->pixmap("text_buttons/ok.png"),
        QPalette().color(QPalette::BrightText)));

    setPaletteColor(m_ui->commentLineEdit, QPalette::PlaceholderText,
        QPalette().color(QPalette::Midlight));
}

void VmsRulesFooterWidget::setComment(const QString& comment)
{
    m_ui->commentLineEdit->setText(comment.trimmed());
    m_ui->commentLineEdit->textEdited(comment);
}

QString VmsRulesFooterWidget::comment() const
{
    return m_ui->commentLineEdit->text();
}

} // namespace rules
} // namespace nx::vms::client::desktop
