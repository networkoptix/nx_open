#ifndef QN_RESOURCE_SELECTION_DIALOG_DELEGATE_H
#define QN_RESOURCE_SELECTION_DIALOG_DELEGATE_H

#include <QtCore/QObject>
#include <QtWidgets/QLabel>

#include <business/business_resource_validator.h>
#include <core/resource/resource_fwd.h>
#include <ui/style/warning_style.h>

class QnResourceSelectionDialogDelegate: public QObject {
    Q_OBJECT
public:
    explicit QnResourceSelectionDialogDelegate(QObject* parent = NULL);
    ~QnResourceSelectionDialogDelegate();

    /**
     * @brief init                  This method allows delegate setup custom error or statistics widget
     *                              that will be displayed below the resources list.
     * @param parent                Parent container widget.
     */
    virtual void init(QWidget* parent);

    /**
     * @brief validate              This method allows delegate modify message depending on the selection
     *                              and control the OK button status.
     * @param selectedResources     List of selected resources.
     * @return                      True if selection is valid and OK button can be pressed.
     */
    virtual bool validate(const QnResourceList &selectedResources);

    virtual bool isValid(const QnResourcePtr &resource);
};

template<typename CheckingPolicy>
class QnCheckResourceAndWarnDelegate: public QnResourceSelectionDialogDelegate, private CheckingPolicy {

    using CheckingPolicy::emptyListIsValid;
    using CheckingPolicy::isResourceValid;
    using CheckingPolicy::getErrorText;

    typedef typename CheckingPolicy::resource_type ResourceType;
    typedef QnResourceSelectionDialogDelegate base_type;
public:
//    QnCheckResourceAndWarnDelegate(QWidget* parent = NULL);
//    ~QnCheckResourceAndWarnDelegate();

//    void init(QWidget* parent) override;
//    bool validate(const QnResourceList &selected) override;
//    bool isValid(const QnResourcePtr &resource) override;


    QnCheckResourceAndWarnDelegate(QWidget* parent):
        QnResourceSelectionDialogDelegate(parent),
        m_warningLabel(NULL)
    {}
    ~QnCheckResourceAndWarnDelegate() {}

    void init(QWidget* parent) override {
        m_warningLabel = new QLabel(parent);
        setWarningStyle(m_warningLabel);
        parent->layout()->addWidget(m_warningLabel);
    }

    bool validate(const QnResourceList &selected) override {
        if (!m_warningLabel)
            return true;

        QnSharedResourcePointerList<ResourceType> filtered = selected.filtered<ResourceType>();
        int invalid = 0;
        foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
            if (!isResourceValid(resource))
                invalid++;

        m_warningLabel->setText(getErrorText(invalid, filtered.size()));
        m_warningLabel->setVisible(invalid > 0 || !emptyListIsValid());
        return true;
    }

    bool isValid(const QnResourcePtr &resource) override {
        QnSharedResourcePointer<ResourceType> derived = resource.dynamicCast<ResourceType>();

        // return true for resources of other type - so root elements will not be highlighted
        return !derived || isResourceValid(derived);
    }
private:
    QLabel* m_warningLabel;
};


#endif // RESOURCE_SELECTION_DIALOG_DELEGATE_H
