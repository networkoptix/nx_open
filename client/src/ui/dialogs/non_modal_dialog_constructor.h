#ifndef QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
#define QN_NON_MODAL_DIALOG_CONSTRUCTOR_H

/** Helper class to restore geometry of the persistent dialogs. */
template<typename T>
class QnNonModalDialogConstructor {
public:
    
    QnNonModalDialogConstructor<T>(QPointer<T> &dialog, QWidget* parent):
        m_newlyCreated(false)
    {
        if (!dialog) {
            dialog = new T(parent);
            m_newlyCreated = true;
        }
        m_dialog = dialog;
        m_oldGeometry = dialog->geometry();
    }

    ~QnNonModalDialogConstructor() {
        m_dialog->show();
        m_dialog->raise();
        m_dialog->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
        if (!m_newlyCreated)
            m_dialog->setGeometry(m_oldGeometry);
    }

private:
    bool m_newlyCreated;
    QRect m_oldGeometry;
    QPointer<T> m_dialog;
};

#endif //QN_NON_MODAL_DIALOG_CONSTRUCTOR_H