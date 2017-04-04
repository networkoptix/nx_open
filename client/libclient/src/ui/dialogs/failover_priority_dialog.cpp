#include "failover_priority_dialog.h"

#include <QtCore/QCoreApplication>

#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/common/palette.h>
#include <ui/delegates/failover_priority_resource_model_delegate.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource/resource_tree_model.h>

namespace {

const int dialogMinimumWidth = 800;

class QnFailoverPriorityDialogDelegate: public QnResourceSelectionDialogDelegate
{
    typedef QnResourceSelectionDialogDelegate base_type;

    Q_DECLARE_TR_FUNCTIONS(QnFailoverPriorityDialogDelegate);
public:
    typedef std::function<void(Qn::FailoverPriority)> ButtonCallback;

    QnFailoverPriorityDialogDelegate(
        ButtonCallback callback,
        QnFailoverPriorityResourceModelDelegate* customColumnDelegate,
        QWidget* parent):

        base_type(parent)
        , m_placeholder(nullptr)
        , m_hintPage(nullptr)
        , m_buttonsPage(nullptr)
        , m_callback(callback)
        , m_customColumnDelegate(customColumnDelegate)
    {
    }

    ~QnFailoverPriorityDialogDelegate()
    {
    }

    virtual void init(QWidget* parent) override
    {
        if (!checkFrame(parent))
            return;

        parent->layout()->setSpacing(0);
        parent->layout()->addWidget(initLine(parent));
        m_placeholder = initPlacehoder(parent);
        parent->layout()->addWidget(m_placeholder);

        QHBoxLayout* layout = new QHBoxLayout(m_buttonsPage);

        QLabel* label = new QLabel(tr("Set Priority:"), parent);
        layout->addWidget(label);

        for (int i = 0; i < Qn::FP_Count; ++i)
        {
            Qn::FailoverPriority priority = static_cast<Qn::FailoverPriority>(i);
            auto button = new QPushButton(QnFailoverPriorityDialog::priorityToString(priority), parent);
            setPaletteColor(button, QPalette::Window, qApp->palette().color(QPalette::Window));
            connect(button, &QPushButton::clicked, this, [this, priority]() { m_callback(priority); });
            layout->addWidget(button);
        }

        layout->addStretch();
    }

    virtual bool validate(const QSet<QnUuid>& selected) override
    {
        if (!m_placeholder)
            return true;

        auto resources = resourcePool()->getResources<QnVirtualCameraResource>(selected);
        bool visible = !resources.isEmpty();
        m_placeholder->setCurrentWidget(visible
            ? m_buttonsPage
            : m_hintPage);
        return true;
    }

    virtual bool isFlat() const override
    {
        return true;
    }

    virtual int helpTopicId() const override
    {
        return Qn::ServerSettings_Failover_Help;
    }

    virtual QnResourceTreeModelCustomColumnDelegate* customColumnDelegate() const override
    {
        return m_customColumnDelegate;
    }

private:
    bool checkFrame(QWidget* parent)
    {
        NX_ASSERT(m_callback, Q_FUNC_INFO, "Callback must be set here");
        if (!m_callback)
            return false;

        NX_ASSERT(parent && parent->layout(), Q_FUNC_INFO, "Invalid delegate frame");
        if (!parent || !parent->layout())
            return false;

        return true;
    }

    QWidget* initLine(QWidget* parent)
    {
        QFrame* line = new QFrame(parent);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        setPaletteColor(line, QPalette::Window, qApp->palette().color(QPalette::Base));
        line->setAutoFillBackground(true);
        return line;
    }

    QStackedWidget* initPlacehoder(QWidget* parent)
    {
        QStackedWidget* placeholder = new QStackedWidget(parent);
        placeholder->setContentsMargins(0, 0, 0, 0);
        setPaletteColor(placeholder, QPalette::Window, qApp->palette().color(QPalette::Base));
        placeholder->setAutoFillBackground(true);

        const QString hint = QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Select devices to setup failover priority"),
            tr("Select cameras to setup failover priority")
        );

        m_hintPage = new QLabel(hint, placeholder);
        m_hintPage->setAlignment(Qt::AlignCenter);
        placeholder->addWidget(m_hintPage);
        m_buttonsPage = new QWidget(placeholder);
        placeholder->addWidget(m_buttonsPage);

        return placeholder;
    }

private:
    QStackedWidget* m_placeholder;
    QLabel* m_hintPage;
    QWidget* m_buttonsPage;

    ButtonCallback m_callback;
    QnResourceTreeModelCustomColumnDelegate* m_customColumnDelegate;
};

}


QnFailoverPriorityDialog::QnFailoverPriorityDialog(QWidget* parent /*= nullptr*/):
    base_type(QnResourceSelectionDialog::Filter::cameras, parent),
    m_customColumnDelegate(new QnFailoverPriorityResourceModelDelegate(this))
{
    setWindowTitle(tr("Failover Priority"));
    setMinimumWidth(dialogMinimumWidth);
    setDelegate(new QnFailoverPriorityDialogDelegate([this](Qn::FailoverPriority priority)
    {
        updatePriorityForSelectedCameras(priority);
    },
        m_customColumnDelegate,
        this));
    setHelpTopic(this, Qn::ServerSettings_Failover_Help);


    auto connectToCamera = [this](const QnVirtualCameraResourcePtr &camera)
    {
        connect(camera, &QnVirtualCameraResource::failoverPriorityChanged, m_customColumnDelegate, &QnResourceTreeModelCustomColumnDelegate::notifyDataChanged);
    };

    for (const QnVirtualCameraResourcePtr &camera : resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        connectToCamera(camera);
    }

    connect(resourcePool(), &QnResourcePool::resourceAdded, this, [connectToCamera](const QnResourcePtr &resource)
    {
        if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
            connectToCamera(camera);
    });
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource)
    {
        disconnect(resource, nullptr, this, nullptr);
    });
}

QString QnFailoverPriorityDialog::priorityToString(Qn::FailoverPriority priority)
{
    switch (priority)
    {
        case Qn::FP_Never:
            return tr("Never", "Failover priority");
        case Qn::FP_Low:
            return tr("Low", "Failover priority");
        case Qn::FP_Medium:
            return tr("Medium", "Failover priority");
        case Qn::FP_High:
            return tr("High", "Failover priority");
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            break;
    }
    return QString();
}

const QnFailoverPriorityColors & QnFailoverPriorityDialog::colors() const
{
    return m_customColumnDelegate->colors();
}

void QnFailoverPriorityDialog::setColors(const QnFailoverPriorityColors &colors)
{
    m_customColumnDelegate->setColors(colors);
}


void QnFailoverPriorityDialog::updatePriorityForSelectedCameras(Qn::FailoverPriority priority)
{
    auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(selectedResources());
    m_customColumnDelegate->forceCamerasPriority(cameras, priority);
    setSelectedResources(QSet<QnUuid>());
}

void QnFailoverPriorityDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    if (button != QDialogButtonBox::Ok)
        return;

    /* We should update default cameras to old value - what have user seen in the dialog. */
    auto forced = m_customColumnDelegate->forcedCamerasPriorities();

    /* Update all default cameras and all cameras that we have changed. */
    auto modified = resourcePool()->getAllCameras(QnResourcePtr(), true).filtered([this, &forced](const QnVirtualCameraResourcePtr &camera)
    {
        return forced.contains(camera->getId());
    });

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified, [this, &forced](const QnVirtualCameraResourcePtr &camera)
    {
        if (forced.contains(camera->getId()))
            camera->setFailoverPriority(forced[camera->getId()]);
        else
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
    });
}
