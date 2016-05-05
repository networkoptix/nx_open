#pragma once

#include <QtWidgets/QWidget>

class QnAbstractPreferencesWidget: public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    explicit QnAbstractPreferencesWidget(QWidget *parent = 0);

    /**
     * @brief hasChanges                        Check if there are modified values.
     *                                          This method must be implemented in derived classes.
     * @return                                  True if something is changed, false otherwise.
     */
    virtual bool hasChanges() const = 0;

    /**
     * @brief loadDataToUi                      Read widget elements' values from model data.
     *                                          This method must be implemented in derived classes.
     */
    virtual void loadDataToUi() = 0;

    /**
     * @brief applyChanges                      Save widget elements' values to model data.
     *                                          This method must be implemented in derived classes.
     */
    virtual void applyChanges() = 0;

    /**
    * @brief discardChanges                    Discard changes if needed. Called only on force closing.
    */
    virtual void discardChanges();

    /**
     * @brief canApplyChanges                   Check that all values are correct so saving is possible.
     *                                          This method is optional, usually it shouldn't be reimplemented.
     * @return                                  False if saving should be aborted, true otherwise.
     */
    virtual bool canApplyChanges() const;

    /**
     * @brief canDiscardChanges                 Check that all values can be discarded safely.
     *                                          This method is optional, usually it shouldn't be reimplemented.
     * @return                                  False if discarding should be aborted, true otherwise.
     */
    virtual bool canDiscardChanges() const;

    /**
     * @brief retranslateUi                     Update ui strings (if required).
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
