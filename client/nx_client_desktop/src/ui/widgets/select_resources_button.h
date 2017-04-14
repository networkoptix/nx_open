#pragma once

#include <QtWidgets/QPushButton>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

/**
* Base class for push buttons displaying currently selected resources.
* It's supposed that click on these buttons will invoke resource selection by the user.
*/
class QnSelectResourcesButton : public QPushButton, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    QnSelectResourcesButton(QWidget* parent = nullptr);

    void selectAny();
    void selectAll();
    void selectNone();

    struct SingleSelectionParameters
    {
        bool showName;
        bool showState;

        SingleSelectionParameters(bool showName, bool showState);
        bool operator == (const SingleSelectionParameters& other) const;
    };

    SingleSelectionParameters singleSelectionParameters() const;
    void setSingleSelectionParameters(const SingleSelectionParameters& parameters);

protected:
    struct Appearance
    {
        QIcon icon;
        QString text;
    };

    /* PURE VIRTUAL OVERRIDABLES: */
    virtual Appearance appearanceForAny() const = 0;
    virtual Appearance appearanceForAll() const = 0;
    virtual Appearance appearanceForSelected(int count) const = 0;

    virtual Appearance appearanceForResource(const QnResourcePtr& resource) const;

    template<class ResourceListType>
    void selectList(const ResourceListType& list)
    {
        int count = list.size();
        if (count == 1)
            setAppearance(appearanceForResource(list[0]));
        else
            setAppearance(appearanceForSelected(count));
    }

private:
    void setAppearance(const Appearance& appearance);

private:
    SingleSelectionParameters m_singleSelectionParameters;
};

/**
* Implementation for button displaying selected cameras/devices.
*/
class QnSelectDevicesButton : public QnSelectResourcesButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesButton;

public:
    QnSelectDevicesButton(QWidget* parent = nullptr);
    void selectDevices(const QnVirtualCameraResourceList& devices);

protected:
    virtual Appearance appearanceForAny() const override;
    virtual Appearance appearanceForAll() const override;
    virtual Appearance appearanceForSelected(int count) const override;
};

/**
* Implementation for button displaying selected users.
*/
class QnSelectUsersButton : public QnSelectResourcesButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesButton;

public:
    QnSelectUsersButton(QWidget* parent = nullptr);
    void selectUsers(const QnUserResourceList& users);

protected:
    virtual Appearance appearanceForAny() const override;
    virtual Appearance appearanceForAll() const override;
    virtual Appearance appearanceForSelected(int count) const override;
};

/**
* Implementation for button displaying selected servers.
*/
class QnSelectServersButton : public QnSelectResourcesButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesButton;

public:
    QnSelectServersButton(QWidget* parent = nullptr);
    void selectServers(const QnMediaServerResourceList& servers);

protected:
    virtual Appearance appearanceForAny() const override;
    virtual Appearance appearanceForAll() const override;
    virtual Appearance appearanceForSelected(int count) const override;
};
