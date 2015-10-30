#ifndef ABSTRACT_PREFERENCES_WIDGET_H
#define ABSTRACT_PREFERENCES_WIDGET_H

#include <QtWidgets/QWidget>

class QnAbstractPreferencesWidget: public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    explicit QnAbstractPreferencesWidget(QWidget *parent = 0);

    /**
     * @brief submitToSettings                  Save widget elements' values to client settings.
     */
    virtual void submitToSettings();

    /**
     * @brief updateFromSettings                Read widget elements' values from client settings.
     */
    virtual void updateFromSettings();

    /**
     * @brief confirm                           Check that all values are correct so saving is possible.
     * @return                                  False if saving should be aborted, true otherwise.
     */
    virtual bool confirm();

    /**
     * @brief discard                           Check that all values can be discarded safely.
     * @return                                  False if discarding should be aborted, true otherwise.
     */
    virtual bool discard();

    /**
     * @brief hasChanges                        Check if there are modified values.
     * @return                                  True if something is changed, false otherwise.
     */
    virtual bool hasChanges() const;

    /**
     * @brief retranslateUi                     Update UI strings (if required).
     */
    virtual void retranslateUi();

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    /** Signal is emitted whenever hasChanges() is changed. */
    void hasChangesChanged();

protected:
    virtual void setReadOnlyInternal(bool readOnly);

private:
    bool m_readOnly;
};

#endif // ABSTRACT_PREFERENCES_WIDGET_H
