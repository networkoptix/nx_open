#include "server_status_delegate.h"
#include "server_updates_model.h"

#include <QtWidgets/QStyleOptionProgressBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QPainter>

#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

namespace
{
    const int kDefaultWidth = 400;
    const int kCellMargin = 4;
} // namespace


namespace nx {
namespace client {
namespace desktop {

using StatusCode = nx::UpdateStatus::Code;

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
        m_right = new QPushButton(this);
        m_right->setFlat(true);
        layout->addWidget(m_right);
        layout->setAlignment(m_right, Qt::AlignRight);

        setLayout(layout);
        setMinimumWidth(200);

        connect(m_right, &QPushButton::clicked,
            this, &ServerStatusWidget::at_clicked);

        connect(owner->m_updateAnimation.data(), &QMovie::frameChanged,
            this, &ServerStatusWidget::at_changeAnimationFrame);
    }

    void setState(UpdateItemPtr data)
    {
        bool errorStyle = false;
        bool progressHidden = true;

        m_animated = false;

        if (data->offline)
        {
            m_left->setText(tr("Skipped"));
            m_left->setIcon(qnSkin->icon("text_buttons/skipped_disabled.png"));
            m_left->setHidden(false);
            m_right->setHidden(true);
        }
        else if (data->skipped)
        {
            m_left->setText(tr("Skipped"));
            m_left->setIcon(qnSkin->icon("text_buttons/skipped_disabled.png"));
            m_left->setHidden(false);

            m_right->setText(tr("Download"));
            m_right->setIcon(qnSkin->icon("text_buttons/download.png"));
            m_right->setHidden(false);
        }
        else
        {
            switch (data->state)
            {
            /* This state is removed
            case StatusCode::checking:
                m_left->setHidden(false);
                m_left->setText(tr("Checking for updates..."));
                //m_left->setIcon(qnSkin->icon("text_buttons/refresh.png"));
                m_animated = true;
                m_left->setIcon(m_owner->getCurrentAnimationFrame());
                m_right->setHidden(true);
                break;*/
            case StatusCode::preparing:
                m_left->setHidden(true);
                progressHidden = false;
                m_progress->setMinimum(0);
                m_progress->setMaximum(0);
                m_right->setText("");
                m_right->setIcon(qnSkin->icon("text_buttons/clear.png"));
                m_right->setHidden(false);
                break;
            case StatusCode::downloading:
                m_left->setHidden(true);
                progressHidden = false;
                m_progress->setMinimum(0);
                m_progress->setMaximum(100);
                m_progress->setValue(data->progress);
                m_right->setText("");
                m_right->setIcon(qnSkin->icon("text_buttons/clear.png"));
                m_right->setHidden(false);
                break;
            /* Is missing for some reason
            case StatusCode::installing:
                m_left->setHidden(false);
                m_left->setText(tr("Installing updates..."));
                //m_left->setIcon(qnSkin->icon("text_buttons/refresh.png"));
                m_animated = true;
                m_left->setIcon(m_owner->getCurrentAnimationFrame());
                m_right->setHidden(true);
                break;*/
            case StatusCode::readyToInstall:
                // TODO: We should get proper server version here
                m_left->setText(tr("Downloaded"));
                m_left->setIcon(qnSkin->icon("text_buttons/ok.png"));
                m_left->setHidden(false);
                m_right->setHidden(true);
                break;

            case StatusCode::error:
                m_left->setText(data->statusMessage);
                m_left->setIcon(qnSkin->icon("text_buttons/clear_error.png"));
                m_left->setHidden(false);

                m_right->setText(tr("Try again"));
                m_right->setIcon(qnSkin->icon("text_buttons/refresh.png"));
                m_right->setHidden(false);

                errorStyle = true;
                break;
            /* Is missing for some reason
            case StatusCode::notAvailable:
                m_left->setHidden(false);
                m_left->setText(tr("Update is not available"));
                errorStyle = true;
                break;*/
            default:
                // In fact we should not be here. All the states should be handled accordingly
                m_left->setHidden(false);
                m_left->setText(lit("Unhandled state: ") + data->statusMessage);
                errorStyle = true;
                break;
            }
        }

        if (errorStyle)
            setWarningStyle(m_left);
        else
            resetStyle(m_left);

        if (progressHidden != m_progress->isHidden())
            m_progress->setHidden(progressHidden);

        if (m_lastItem != data)
        {
            m_lastItem = data;
        }
    }

protected:
    void at_clicked()
    {
        if (m_owner && m_lastItem)
            emit m_owner->updateItemCommand(m_lastItem);
    }

    void at_changeAnimationFrame(int frame)
    {
        if (!m_animated)
            return;
        m_left->setIcon(m_owner->getCurrentAnimationFrame());
    }

private:
    QPushButton* m_left;
    QProgressBar* m_progress;
    QPushButton* m_right;

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
    /*
    const auto movie = qnSkin->newMovie("legacy/loading.gif", button);
    connect(movie, &QMovie::frameChanged, button,
    [button, movie](int frameNumber)
    {
    button->setIcon(movie->currentPixmap());
    });
    */
}

ServerStatusItemDelegate::~ServerStatusItemDelegate()
{
}

QPixmap ServerStatusItemDelegate::getCurrentAnimationFrame() const
{
    NX_ASSERT(m_updateAnimation);
    return m_updateAnimation ? m_updateAnimation->currentPixmap() : QPixmap();
}

QWidget* ServerStatusItemDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    return new ServerStatusWidget(this, parent);
}

void ServerStatusItemDelegate::setEditorData(QWidget* editor, const QModelIndex &index) const
{
    ServerStatusWidget* statusWidget = dynamic_cast<ServerStatusWidget*>(editor);
    if (!statusWidget)
        return;

    auto itemData = index.data(UpdateItem::UpdateItemRole);
    //UpdateItemPtr data = reinterpret_cast<UpdateItem*>(itemData.value<void*>());
    UpdateItemPtr data = itemData.value<UpdateItemPtr>();
    if (!data)
        return;
    statusWidget->setState(data);
}

} // namespace desktop
} // namespace client
} // namespace nx
