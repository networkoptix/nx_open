#pragma once

#include <QtWidgets/QPushButton>

#include <core/resource/resource_fwd.h>

/**
* Base class for push buttons displaying currently selected resources.
* It's supposed that click on these buttons will invoke resource selection by the user.
*/
class QnSelectResourcesButton : public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    QnSelectResourcesButton(QWidget* parent = nullptr);

    void selectOne(const QnResourcePtr& one);

    void selectAny();
    void selectAll();

    template<class ResourceListType>
    void selectFew(const ResourceListType& few)
    {
        int count = few.size();
        if (count == 1)
            setAppearance(appearanceForResource(few[0]));
        else
            setAppearance(appearanceForSelected(count));
    }

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

    virtual Appearance appearanceForAny() = 0;
    virtual Appearance appearanceForAll() = 0;
    virtual Appearance appearanceForSelected(int count) = 0;

    virtual Appearance appearanceForResource(const QnResourcePtr& resource);

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

protected:
    virtual Appearance appearanceForAny() override;
    virtual Appearance appearanceForAll() override;
    virtual Appearance appearanceForSelected(int count) override;
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

protected:
    virtual Appearance appearanceForAny() override;
    virtual Appearance appearanceForAll() override;
    virtual Appearance appearanceForSelected(int count) override;
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

protected:
    virtual Appearance appearanceForAny() override;
    virtual Appearance appearanceForAll() override;
    virtual Appearance appearanceForSelected(int count) override;
};
