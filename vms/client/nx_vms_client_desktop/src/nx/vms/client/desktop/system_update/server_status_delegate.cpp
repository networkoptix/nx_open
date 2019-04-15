#include "server_status_delegate.h"
#include "server_updates_model.h"

#include <QtWidgets/QStyleOptionProgressBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QPainter>

#include <nx/utils/log/assert.h>
#include <nx/utils/literal.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

namespace nx::vms::client::desktop {

using StatusCode = nx::update::Status::Code;

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

        m_animated = false;
        if (m_owner->isVerificationErrorVisible())
        {
            // Not showing any other statuses if there are any verification errors.
            if (!data->verificationMessage.isEmpty())
            {
                m_left->setText(data->verificationMessage);
                m_left->setIcon(qnSkin->icon("text_buttons/clear_error.png"));
                leftHidden = false;
                errorStyle = true;
            }
            else
            {
                leftHidden = true;
            }
        }
        else if (data->skipped)
        {
            m_left->setText(tr("Skipped"));
            m_left->setIcon(qnSkin->icon("text_buttons/ok.png"));
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
            m_left->setIcon(qnSkin->icon("text_buttons/ok.png"));
            leftHidden = false;
        }
        else if (data->installing)
        {
            m_left->setText(tr("Installing..."));
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
                    m_left->setIcon(qnSkin->icon("text_buttons/ok.png"));
                    leftHidden = false;
                    break;
                case StatusCode::error:
                    m_left->setText(data->statusMessage);
                    m_left->setIcon(qnSkin->icon("text_buttons/clear_error.png"));
                    leftHidden = false;
                    errorStyle = true;
                    break;
                case StatusCode::latestUpdateInstalled:
                    m_left->setText(tr("Installed"));
                    m_left->setIcon(qnSkin->icon("text_buttons/ok.png"));
                    leftHidden = false;
                    break;
                case StatusCode::idle:
                    leftHidden = true;
                    progressHidden = true;
                    break;
                case StatusCode::offline:
                    // TODO: Some servers that are installing updates can also be offline. But it is different state
                    m_left->setText("–");
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

        if (m_lastItem != data)
        {
            m_lastItem = data;
        }
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
    UpdateItemPtr m_lastItem;
};

ServerStatusItemDelegate::ServerStatusItemDelegate(QWidget* parent) :
    QStyledItemDelegate(parent)
{
    m_updateAnimation.reset(qnSkin->newMovie("legacy/loading.gif", this));
    // Loop this
    if (m_updateAnimation->loopCount() != -1)
        connect(m_updateAnimation.data(), &QMovie::finished, m_updateAnimation.data(), &QMovie::start);

    m_updateAnimation->start();
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
