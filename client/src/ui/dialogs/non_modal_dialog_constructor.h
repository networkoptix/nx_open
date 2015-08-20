#ifndef QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
#define QN_NON_MODAL_DIALOG_CONSTRUCTOR_H

class QnShowDialogHelper {
public:
    static void show(QWidget* dialog, const QRect &targetGeometry);

    static void showNonModalDialog(QWidget *dialog
        , const QRect &targetGeometry = QRect()
        , bool dontFocus = false);

    static QPoint calculatePosition(QWidget* dialog);
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

    /** Invalidate stored geometry so dialog will be displayed in the center of the screen. */
    void resetGeometry() {
        m_targetGeometry = QRect();
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

    QnNonModalDialogConstructor<T>(QPointer<T> &dialog, QWidget* parent) :
        m_dontFocus(true)
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
        showNonModalDialog(m_dialog, m_targetGeometry, m_dontFocus);
    }

    /** Invalidate stored geometry so dialog will be displayed in the center of the screen. */
    void resetGeometry() {
        m_targetGeometry = QRect();
    }

    void setDontFocus(bool f) {
        m_dontFocus = f;
    }

private:
    QRect m_targetGeometry;
    QPointer<T> m_dialog;
    bool m_dontFocus;
};

#endif //QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
