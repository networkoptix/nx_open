#pragma once

#include <QtWidgets/QWidget>

class QnAbstractPreferencesInterface
{
public:
    QnAbstractPreferencesInterface() = default;
    virtual ~QnAbstractPreferencesInterface() = default;

    /**
     * Check if there are modified values.
     * @return True if something is changed, false otherwise.
     */
    virtual bool hasChanges() const = 0;

    /**
     * Read widget elements' values from model data.
     */
    virtual void loadDataToUi() = 0;

    /**
     * Save widget elements' values to model data.
     */
    virtual void applyChanges() = 0;

    /**
     * Discard changes if needed. Called only on force closing.
     */
    virtual void discardChanges() {}

    /**
     * Check that all values are correct so saving is possible.
     * @return False if saving should be aborted, true otherwise.
     */
    virtual bool canApplyChanges() const
    {
        return true;
    }

    /**
     * Check that all values can be discarded safely.
     * @return False if discarding should be aborted, true otherwise.
     */
    virtual bool canDiscardChanges() const
    {
        return true;
    }
};

class QnAbstractPreferencesWidget: public QWidget, public QnAbstractPreferencesInterface
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;
public:
    explicit QnAbstractPreferencesWidget(QWidget* parent = nullptr);

    /**
     * Update ui strings (if required).
     */
    virtual void retranslateUi();

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    /** Signal is emitted whenever hasChanges() is changed. */
    void hasChangesChanged();

protected:
    virtual void setReadOnlyInternal(bool readOnly) {}

private:
    bool m_readOnly = false;
};
