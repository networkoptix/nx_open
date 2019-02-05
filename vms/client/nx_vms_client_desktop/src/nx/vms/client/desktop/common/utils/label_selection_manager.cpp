#include "label_selection_manager.h"

#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/private/qwidgettextcontrol_p.h>

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// LabelSelectionManager::Private

class LabelSelectionManager::Private: public QObject
{
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
            case QEvent::MouseButtonPress:
            {
                const auto mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() != Qt::LeftButton)
                    break;

                const auto focusLabel = qobject_cast<QLabel*>(QApplication::focusWidget());
                if (focusLabel && focusLabel != label)
                    focusLabel->clearFocus();

                break;
            }

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
                [text = label->selectedText()]() { QApplication::clipboard()->setText(text); });
        }
        else
        {
            menu->addAction(tr("Copy Link Location"),
                [linkText]() { QApplication::clipboard()->setText(linkText); });
        }

        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
        return true;
    }

    static QWidgetTextControl* getTextControl(QLabel* label)
    {
        return label->findChild<QWidgetTextControl*>({}, Qt::FindDirectChildrenOnly);
    }

    static void setFullSelection(QWidgetTextControl* textControl)
    {
        auto cursor = textControl->textCursor();
        cursor.select(QTextCursor::Document);
        textControl->setTextCursor(cursor);
    }
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
