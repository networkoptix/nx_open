#ifndef QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
#define QN_NON_MODAL_DIALOG_CONSTRUCTOR_H

class QnShowDialogHelper {
public:
    static void show(QWidget* dialog, const QRect &targetGeometry) {
        dialog->show();
        dialog->raise();
        dialog->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
        if (!targetGeometry.isNull())
            dialog->setGeometry(targetGeometry);
    }
};

/** Helper class to show target non-modal dialog as soon as top-level modal dialog is closed. */
class QnDelayedShowHelper: public QObject, public QnShowDialogHelper {
    Q_OBJECT
public:
    QnDelayedShowHelper(QWidget* targetWidget, const QRect &targetGeometry, int sourceCount, QObject *parent = NULL):
        QObject(parent),
        m_targetWidget(targetWidget),
        m_targetGeometry(targetGeometry),
        m_sourceCount(sourceCount)
    {}
    
    virtual bool eventFilter(QObject *watched, QEvent *event) override {
        if (m_targetWidget && event->type() == QEvent::Hide) {
            watched->removeEventFilter(this);   //avoid double call
            m_sourceCount--;

            if (m_sourceCount <= 0) {
                show(m_targetWidget.data(), m_targetGeometry);
                deleteLater();
            }
            
        }

        return QObject::eventFilter(watched, event);
    }


private:
    QPointer<QWidget> m_targetWidget;
    QRect m_targetGeometry;
    int m_sourceCount;
};


/** Helper class to restore geometry of the persistent dialogs. */
template<typename T>
class QnNonModalDialogConstructor: public QnShowDialogHelper {
public:
    
    QnNonModalDialogConstructor<T>(QPointer<T> &dialog, QWidget* parent)
    {
        if (!dialog) {
            dialog = new T(parent);
        } else {
            /* Dialog's show() method will reset the geometry, saving it to restore afterwards. */
            m_targetGeometry = dialog->geometry();  
        }
        m_dialog = dialog;
    }

    ~QnNonModalDialogConstructor() {
        QWidgetList modalWidgets;

        /* Checking all top-level widgets. */
        foreach (QWidget* tlw, qApp->topLevelWidgets()) {
            if (!tlw->isHidden() && tlw->isModal())
                modalWidgets << tlw;
        }

        /* Setup dialog to show after all modal windows are closed. */
        if (!modalWidgets.isEmpty()) {
            QnDelayedShowHelper* helper = new QnDelayedShowHelper(m_dialog, m_targetGeometry, modalWidgets.size(), m_dialog);
            foreach (QWidget* modalWidget, modalWidgets)
                modalWidget->installEventFilter(helper);
            return;
        }

        /* Or show it immediately if no modal windows are visible. */
        show(m_dialog, m_targetGeometry);
    }

private:
    QRect m_targetGeometry;
    QPointer<T> m_dialog;
};

#endif //QN_NON_MODAL_DIALOG_CONSTRUCTOR_H