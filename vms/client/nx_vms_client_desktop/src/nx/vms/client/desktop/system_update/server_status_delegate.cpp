// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_status_delegate.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>

#include <QtWidgets/QStyleOptionProgressBar>

#include <nx/utils/literal.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/custom_style.h>

#include "server_updates_model.h"

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconTheme = {
    {QIcon::Normal, {.primary = "light10"}},
    {QIcon::Active, {.primary = "light11"}},
};

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconErrorTheme = {
    {QIcon::Normal, {.primary = "red"}},
};

NX_DECLARE_COLORIZED_ICON(kCrossCloseIcon, "20x20/Outline/cross_close.svg", kIconErrorTheme)
NX_DECLARE_COLORIZED_ICON(kSuccessIcon, "20x20/Outline/check.svg", kIconTheme)

} // namespace

namespace nx::vms::client::desktop {

using StatusCode = nx::vms::common::update::Status::Code;

class ServerStatusItemDelegate::ServerStatusWidget: public QWidget
{
public:
    ServerStatusWidget(const ServerStatusItemDelegate* owner, QWidget* parent):
        QWidget(parent),
        m_owner(owner)
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        m_left = new QPushButton(this);
        m_left->setFlat(true);
        layout->addWidget(m_left);
        layout->setAlignment(m_left, Qt::AlignLeft);
        m_progress = new QProgressBar(this);
        m_progress->setTextVisible(false);
        layout->addWidget(m_progress);
        setLayout(layout);
        setMinimumWidth(200);

        connect(owner->m_updateAnimation.data(), &QMovie::frameChanged,
            this, &ServerStatusWidget::at_changeAnimationFrame);
    }

    void setState(UpdateItemPtr data)
    {
        bool errorStyle = false;
        bool progressHidden = true;
        bool leftHidden = true;

        static const auto okIcon = qnSkin->icon(kSuccessIcon);

        m_animated = false;
        if (m_owner->isVerificationErrorVisible() && !data->verificationMessage.isEmpty())
        {
            m_left->setText(data->verificationMessage);
            m_left->setIcon(qnSkin->icon(kCrossCloseIcon));
            leftHidden = false;
            errorStyle = true;
        }
        else if (data->skipped)
        {
            m_left->setText(tr("Skipped"));
            m_left->setIcon(okIcon);
            leftHidden = false;
        }
        else if (data->statusUnknown)
        {
            m_left->setText(tr("Waiting for server to respond..."));
            leftHidden = false;
            m_animated = true;
            progressHidden = true;
        }
        else if (data->installed)
        {
            m_left->setText(tr("Installed"));
            m_left->setIcon(okIcon);
            leftHidden = false;
        }
        else if (data->installing)
        {
            m_left->setText(tr("Installing..."));
            leftHidden = false;
            m_animated = true;
            progressHidden = true;
        }
        else if (data->offline && data->state != StatusCode::offline && !data->incompatible)
        {
            m_left->setText(tr("Waiting for server to respond..."));
            leftHidden = false;
            m_animated = true;
            progressHidden = true;
        }
        else
        {
            switch (data->state)
            {
                case StatusCode::preparing:
                    leftHidden = true;
                    progressHidden = false;
                    m_progress->setMinimum(0);
                    m_progress->setMaximum(0);
                    break;
                case StatusCode::downloading:
                    leftHidden = true;
                    progressHidden = false;
                    m_progress->setMinimum(0);
                    m_progress->setMaximum(100);
                    m_progress->setValue(data->progress);
                    break;
                case StatusCode::readyToInstall:
                    // TODO: We should get proper server version here
                    m_left->setText(tr("Downloaded"));
                    m_left->setIcon(okIcon);
                    leftHidden = false;
                    break;
                case StatusCode::error:
                    m_left->setText(data->statusMessage);
                    m_left->setIcon(qnSkin->icon(kCrossCloseIcon));
                    leftHidden = false;
                    errorStyle = true;
                    break;
                case StatusCode::latestUpdateInstalled:
                    m_left->setText(tr("Installed"));
                    m_left->setIcon(okIcon);
                    leftHidden = false;
                    break;
                case StatusCode::idle:
                case StatusCode::starting:
                    leftHidden = true;
                    progressHidden = true;
                    break;
                case StatusCode::offline:
                    // TODO: Some servers that are installing updates can also be offline. But it is different state
                    m_left->setText(nx::UnicodeChars::kEnDash);
                    m_left->setIcon(QIcon());
                    leftHidden = false;
                    break;
                default:
                    // In fact we should not be here. All the states should be handled accordingly
                    leftHidden = false;
                    m_left->setText(QString("Unhandled state: ") + data->statusMessage);
                    errorStyle = true;
                    break;
            }
        }

        if (!m_owner->isStatusVisible())
        {
            progressHidden = true;
            leftHidden = true;
        }

        if (errorStyle)
            setWarningStyle(m_left);
        else
            resetStyle(m_left);

        m_left->setEnabled(!data->offline);

        if (progressHidden != m_progress->isHidden())
            m_progress->setHidden(progressHidden);
        if (leftHidden != m_left->isHidden())
            m_left->setHidden(leftHidden);
    }

protected:
    void at_changeAnimationFrame(int /*frame*/)
    {
        if (!m_animated)
            return;
        m_left->setIcon(m_owner->getCurrentAnimationFrame());
    }

private:
    QPushButton* m_left;
    QProgressBar* m_progress;

    bool m_animated = false;

    QPointer<const ServerStatusItemDelegate> m_owner;
};

ServerStatusItemDelegate::ServerStatusItemDelegate(QWidget* parent) :
    QStyledItemDelegate(parent)
{
    QMovie* const movie = qnSkin->newMovie("legacy/loading.gif", this);

    m_updateAnimation.reset(movie);
    // Loop this
    if (movie->loopCount() != -1)
        connect(movie, &QMovie::finished, movie, &QMovie::start);

    movie->start();
}

ServerStatusItemDelegate::~ServerStatusItemDelegate()
{
}

QPixmap ServerStatusItemDelegate::getCurrentAnimationFrame() const
{
    NX_ASSERT(m_updateAnimation);
    return m_updateAnimation ? m_updateAnimation->currentPixmap() : QPixmap();
}

void ServerStatusItemDelegate::setStatusMode(StatusMode mode)
{
    m_statusMode = mode;
}

bool ServerStatusItemDelegate::isStatusVisible() const
{
    return m_statusMode != StatusMode::hidden;
}

bool ServerStatusItemDelegate::isVerificationErrorVisible() const
{
    return m_statusMode == StatusMode::reportErrors;
}

QWidget* ServerStatusItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    return new ServerStatusWidget(this, parent);
}

void ServerStatusItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    ServerStatusWidget* statusWidget = dynamic_cast<ServerStatusWidget*>(editor);
    if (!statusWidget)
        return;

    auto itemData = index.data(ServerUpdatesModel::UpdateItemRole);
    UpdateItemPtr data = itemData.value<UpdateItemPtr>();
    if (!data)
        return;
    statusWidget->setState(data);
    statusWidget->setVisible(isStatusVisible());
}

} // namespace nx::vms::client::desktop
