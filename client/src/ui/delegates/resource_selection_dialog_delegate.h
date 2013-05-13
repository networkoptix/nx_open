#ifndef QN_RESOURCE_SELECTION_DIALOG_DELEGATE_H
#define QN_RESOURCE_SELECTION_DIALOG_DELEGATE_H

#include <QtCore/QObject>
#include <QtGui/QLabel>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

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
};


template<class ResourceType>
class QnCheckResourceAndWarnDelegate: public QnResourceSelectionDialogDelegate {
    typedef QnResourceSelectionDialogDelegate base_type;
public:
    QnCheckResourceAndWarnDelegate(QWidget* parent);
    ~QnCheckResourceAndWarnDelegate();
    virtual void init(QWidget* parent) override;
    virtual bool validate(const QnResourceList &selected) override;
protected:
    virtual bool isResourceValid(const QnSharedResourcePointer<ResourceType> &resource) const = 0;
    virtual QString getText(int invalid, int total) const = 0;

private:
    QLabel* m_warningLabel;
};

class QnMotionEnabledDelegate: public QnCheckResourceAndWarnDelegate<QnVirtualCameraResource> {
    Q_OBJECT
    typedef QnCheckResourceAndWarnDelegate base_type;

public:
    QnMotionEnabledDelegate(QWidget* parent);
    ~QnMotionEnabledDelegate();

protected:
    virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override;
    virtual QString getText(int invalid, int total) const override;
};


class QnRecordingEnabledDelegate: public QnCheckResourceAndWarnDelegate<QnVirtualCameraResource> {
    Q_OBJECT
    typedef QnCheckResourceAndWarnDelegate base_type;

public:
    QnRecordingEnabledDelegate(QWidget* parent);
    ~QnRecordingEnabledDelegate();

protected:
    virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override;
    virtual QString getText(int invalid, int total) const override;
};


class QnInputEnabledDelegate: public QnCheckResourceAndWarnDelegate<QnVirtualCameraResource> {
    Q_OBJECT
    typedef QnCheckResourceAndWarnDelegate base_type;

public:
    QnInputEnabledDelegate(QWidget* parent);
    ~QnInputEnabledDelegate();

protected:
    virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override;
    virtual QString getText(int invalid, int total) const override;
};


class QnOutputEnabledDelegate: public QnCheckResourceAndWarnDelegate<QnVirtualCameraResource> {
    Q_OBJECT
    typedef QnCheckResourceAndWarnDelegate base_type;

public:
    QnOutputEnabledDelegate(QWidget* parent);
    ~QnOutputEnabledDelegate();

protected:
    virtual bool isResourceValid(const QnVirtualCameraResourcePtr &camera) const override;
    virtual QString getText(int invalid, int total) const override;
};


class QnEmailValidDelegate: public QnCheckResourceAndWarnDelegate<QnUserResource> {
    Q_OBJECT
    typedef QnCheckResourceAndWarnDelegate base_type;

public:
    QnEmailValidDelegate(QWidget* parent);
    ~QnEmailValidDelegate();

protected:
    virtual bool isResourceValid(const QnUserResourcePtr &user) const override;
    virtual QString getText(int invalid, int total) const override;
};

#endif // RESOURCE_SELECTION_DIALOG_DELEGATE_H
