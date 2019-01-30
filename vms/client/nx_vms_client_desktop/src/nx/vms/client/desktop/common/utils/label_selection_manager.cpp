#include "label_selection_manager.h"

#include <QtCore/QChildEvent>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/private/qwidgettextcontrol_p.h>

#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// LabelSelectionManager::Private

class LabelSelectionManager::Private: public QObject
{
    using LabelSet = QSet<QLabel*>;
    static constexpr std::string_view kSelectionsPropertyName = "__qn_LabelsWithSelection";

public:
    explicit Private(QCoreApplication* application)
    {
        if (NX_ASSERT(application))
            application->installEventFilter(this);
    }

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        const auto widget = qobject_cast<QWidget*>(watched);
        if (!widget)
            return false;

        const auto label = qobject_cast<QLabel*>(widget);
        switch (event->type())
        {
            // Register every QWidgetTextControl added as a child to a QLabel.
            case QEvent::ChildAdded:
            {
                if (!label)
                    break;

                auto childEvent = static_cast<QChildEvent*>(event);
                const auto handleChildAdded =
                    [this, child = QPointer<QObject>(childEvent->child())]()
                    {
                        if (const auto textControl = qobject_cast<QWidgetTextControl*>(child))
                            registerTextControl(textControl);
                    };

                // Must check later if the object is QWidgetTextControl, as at this point
                // it's still at the QObject stage of construction.
                executeLater(handleChildAdded, this);
                break;
            }

            // On a left mouse button press within a window clear selection of all registered
            // text controls inside the same window.
            case QEvent::MouseButtonPress:
            {
                const auto mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() != Qt::LeftButton)
                    break;

                clearAll(widget->window());
                m_pressedLabel = label;
                break;
            }

            // On a left mouse button click on a label without selection, select its entire text.
            case QEvent::MouseButtonRelease:
            {
                const auto mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() != Qt::LeftButton)
                    break;

                if (label && label == m_pressedLabel && label->selectedText().isEmpty()
                    && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse))
                {
                    if (const auto textControl = getTextControl(label))
                        setFullSelection(textControl);
                }

                m_pressedLabel = nullptr;
                break;
            }

            // Replace standard Qt popup menus on labels.
            case QEvent::ContextMenu:
            {
                if (label && processContextMenuEvent(label, static_cast<QContextMenuEvent*>(event)))
                {
                    event->accept();
                    return true; //< Stop further event processing.
                }

                break;
            }
        }

        return false;
    }

private:
    void registerTextControl(QWidgetTextControl* textControl)
    {
        if (!NX_ASSERT(textControl) || m_textControls.contains(textControl))
            return;

        connect(textControl, &QWidgetTextControl::selectionChanged, this,
            [this]()
            {
                const auto textControl = qobject_cast<QWidgetTextControl*>(sender());
                if (NX_ASSERT(textControl))
                {
                    const auto label = getLabel(textControl);
                    clearAllExcept(getWindow(textControl), label);

                    if (!label->selectedText().isEmpty())
                        label->setFocus();
                }
            });

        connect(textControl, &QObject::destroyed, this,
            [this](QObject* object)
            {
                m_textControls.remove(static_cast<QWidgetTextControl*>(object));
            });

        m_textControls.insert(textControl);
    }

    void clearAll(QWidget* window)
    {
        clearAllExcept(window, nullptr);
    }

    void clearAllExcept(QWidget* window, QLabel* exception)
    {
        if (m_updating)
            return;

        QScopedValueRollback<bool> update(m_updating, true);
        for (auto control: m_textControls)
        {
            if (getLabel(control) != exception && getWindow(control) == window)
                clearSelection(control);
        }
    }

    bool processContextMenuEvent(QLabel* label, QContextMenuEvent* event) const
    {
        const auto textControl = getTextControl(label);
        if (!textControl)
            return false;

        if (label->text().isEmpty())
            return true;

        auto menu = new QMenu(label);
        menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

        const auto linkText = textControl->anchorAt(event->pos());
        if (linkText.isEmpty())
        {
            if (label->selectedText().isEmpty())
                setFullSelection(textControl);

            menu->addAction(tr("Copy"),
                [text = label->selectedText()]() { qApp->clipboard()->setText(text); });
        }
        else
        {
            menu->addAction(tr("Copy Link Location"),
                [linkText]() { qApp->clipboard()->setText(linkText); });
        }

        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
        return true;
    }

    static QWidgetTextControl* getTextControl(QLabel* label)
    {
        return label->findChild<QWidgetTextControl*>({}, Qt::FindDirectChildrenOnly);
    }

    static QWidget* getWindow(QWidgetTextControl* textControl)
    {
        const auto label = getLabel(textControl);
        return label ? label->window() : nullptr;
    }

    static QLabel* getLabel(QWidgetTextControl* textControl)
    {
        return qobject_cast<QLabel*>(textControl->parent());
    }

    static void clearSelection(QWidgetTextControl* textControl)
    {
        auto cursor = textControl->textCursor();
        cursor.clearSelection();
        textControl->setTextCursor(cursor);
    }

    static void setFullSelection(QWidgetTextControl* textControl)
    {
        auto cursor = textControl->textCursor();
        cursor.select(QTextCursor::Document);
        textControl->setTextCursor(cursor);
    }

private:
    QSet<QWidgetTextControl*> m_textControls;
    QPointer<QLabel> m_pressedLabel;
    bool m_updating = false;
};

//-------------------------------------------------------------------------------------------------
// LabelSelectionManager

LabelSelectionManager::LabelSelectionManager(QCoreApplication* application):
    d(new Private(application))
{
}

LabelSelectionManager::~LabelSelectionManager()
{
}

bool LabelSelectionManager::init(QCoreApplication* application)
{
    if (!NX_ASSERT(application, "LabelSelectionManager requires QCoreApplication instance to exist"))
        return false;

    static LabelSelectionManager instance(application);
    return true;
}

} // namespace nx::vms::client::desktop
