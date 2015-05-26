#ifndef QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
#define QN_NON_MODAL_DIALOG_CONSTRUCTOR_H

class QnShowDialogHelper {
public:
    static void show(QWidget* dialog, const QRect &targetGeometry);

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
class QnNonModalDialogConstructor: public QnShowDialogHelper 
{
    typedef QnNonModalDialogConstructor<T> ThisType;

public:
    typedef QPointer<T> DialogType;
    typedef std::function<T* ()> DialogCreationFunc;
    QnNonModalDialogConstructor(DialogType &dialog
        , QWidget *parent
        , const DialogCreationFunc &creator);

    QnNonModalDialogConstructor(DialogType &dialog
        , QWidget *parent);

    ~QnNonModalDialogConstructor();

    /** Invalidate stored geometry so dialog will be displayed in the center of the screen. */
    void resetGeometry();

    void setDontFocus(bool f);

private:
    /// should be used only from constructor!
    DialogType createAndInitializeDialog(DialogType &output
        , const DialogCreationFunc &creator);

private:
    QRect m_targetGeometry;
    DialogType m_dialog;
    bool m_dontFocus;
};

template<typename T>
QPointer<T> QnNonModalDialogConstructor<T>::createAndInitializeDialog(DialogType &output
    , const DialogCreationFunc &creator)
{
    if (!output) 
    {
        output = creator();
    }
    else 
    {
        /* Dialog's show() method will reset the geometry, saving it to restore afterwards. */
        m_targetGeometry = output->geometry();  
    }
    return output;
}

template<typename T>
QnNonModalDialogConstructor<T>::QnNonModalDialogConstructor(DialogType &dialog
    , QWidget *parent
    , const DialogCreationFunc &creator)
    : m_targetGeometry()
    , m_dialog(createAndInitializeDialog(dialog, creator))
    , m_dontFocus(true)
{
}

template<typename T>
QnNonModalDialogConstructor<T>::QnNonModalDialogConstructor(DialogType &dialog
    , QWidget* parent)
    : m_targetGeometry()
    , m_dialog(createAndInitializeDialog(dialog, [parent](){ return (new T(parent));}))
    , m_dontFocus(true)
{
}

template<typename T>
QnNonModalDialogConstructor<T>::~QnNonModalDialogConstructor() 
{
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
    if (m_dialog->isVisible() && m_dontFocus) {
        m_dialog->raise();
        return;
    }

    show(m_dialog, m_targetGeometry);
}

template<typename T>
void QnNonModalDialogConstructor<T>::resetGeometry() 
{
    m_targetGeometry = QRect();
}

template<typename T>
void QnNonModalDialogConstructor<T>::setDontFocus(bool f) 
{
    m_dontFocus = f;
}

#endif //QN_NON_MODAL_DIALOG_CONSTRUCTOR_H
