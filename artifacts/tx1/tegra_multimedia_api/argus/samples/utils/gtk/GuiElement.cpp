/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>

#include <gtk/gtk.h>

#include <limits>       // for numeric_limits

#include "GuiElement.h"
#include "Error.h"
#include "Value.h"
#include "Validator.h"

namespace ArgusSamples
{

/**
 * RAII helper class for calling g_object_unref on exit from a block or function.
 */
template <typename T> class UniqueGObject
{
public:
    /**
     * Constructor
     */
    UniqueGObject()
        : m_ptr(NULL)
    {
    }

    /**
     * Constructor of object owning 'ptr'.
     */
    explicit UniqueGObject(T *ptr)
        : m_ptr(ptr)
    {
    }

    /**
     * Destructor, Destructs the owned object if such is present.
     */
    ~UniqueGObject()
    {
        reset();
    }

    /**
     * Dereferences pointer to the owned object
     */
    T* get() const
    {
        return m_ptr;
    }

    /**
     * Returns true if there is an owned object.
     */
#if (__cplusplus > 201100L)
    explicit operator bool() const
#else
    operator bool() const
#endif
    {
        return (m_ptr != NULL);
    }

    /**
     * Release the ownership of the object. Return the owned object.
     */
    T* release()
    {
        T *ptr = m_ptr;
        m_ptr = NULL;
        return ptr;
    }

    /**
     * Replace the owned object, the previously owned object is destructed.
     */
    void reset(T *ptr = NULL)
    {
        if (m_ptr)
            g_object_unref(m_ptr);
        m_ptr = ptr;
    }

private:
    T *m_ptr;       ///< owned object

    /**
     * Hide copy constructor and assignment operator
     */
    UniqueGObject(UniqueGObject &right);
    UniqueGObject& operator=(const UniqueGObject &right);
};

/**
 * Create an element holding an already existing widget
 */
static bool createElement(GtkWidget *widget, Window::IGuiElement **element)
{
    UniquePointer<Window::IGuiElement> newElement;

    // Create an element from the widget.
    // The order here is important because e.g. a menu bar is also a container
    if (GTK_IS_MENU(widget))
        newElement.reset(new GuiMenu(widget));
    else if (GTK_IS_MENU_ITEM(widget))
        newElement.reset(new GuiMenuItem(widget));
    else if (GTK_IS_MENU_BAR(widget))
        newElement.reset(new GuiMenuBar(widget));
    else if (GTK_IS_CONTAINER(widget))
        newElement.reset(new GuiContainer(widget));
    else
        newElement.reset(new GuiElement(widget));

    if (!newElement)
        ORIGINATE_ERROR("Out of memory");

    *element = newElement.release();

    return true;
}

WidgetHolder::WidgetHolder()
    : m_widget(NULL)
{
}

WidgetHolder::WidgetHolder(GtkWidget *widget)
    : m_widget(NULL)
{
    setWidget(widget);
}

WidgetHolder::~WidgetHolder()
{
    if (m_widget)
        g_object_unref(m_widget);
}

void WidgetHolder::setWidget(GtkWidget *widget)
{
    if (m_widget)
        g_object_unref(m_widget);
    m_widget = widget;
    g_object_ref_sink(m_widget);
}

GtkWidget *WidgetHolder::getWidget() const
{
    return m_widget;
}

/*static*/ bool Window::IGuiMenuItem::create(const char *name, CallBackFunc function, void *userPtr,
    IGuiMenuItem **iGuiMenuItem)
{
    UniquePointer<GuiMenuItem> newMenuItem(new GuiMenuItem());
    if (!newMenuItem)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newMenuItem->initialize(name, function, userPtr));

    *iGuiMenuItem = newMenuItem.release();

    return true;
}

GuiElementBase::GuiElementBase()
{
}

GuiElementBase::GuiElementBase(GtkWidget *widget)
    : WidgetHolder(widget)
{
}

GuiElementBase::~GuiElementBase()
{
}

GuiMenuItem::GuiMenuItem()
{
}

GuiMenuItem::GuiMenuItem(GtkWidget *widget)
    : GuiElementBase(widget)
    , m_function(NULL)
    , m_userPtr(NULL)
{
}

GuiMenuItem::~GuiMenuItem()
{
}

bool GuiMenuItem::initialize(const char *name, CallBackFunc function, void *userPtr)
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    UniqueGObject<GtkWidget> menuItem(gtk_menu_item_new_with_label(name));
    if (!menuItem)
        ORIGINATE_ERROR("Out of memory");
    setWidget(menuItem.release());

    if (function)
    {
        m_function = function;
        m_userPtr = userPtr;
        g_signal_connect(G_OBJECT(getWidget()), "activate", G_CALLBACK(onActivate),
            this);
    }

    return true;
}

/*static*/ void GuiMenuItem::onActivate(GtkMenuItem *widget, gpointer data)
{
    GuiMenuItem *menuItem = static_cast<GuiMenuItem*>(data);
    PROPAGATE_ERROR_CONTINUE(menuItem->m_function(menuItem->m_userPtr, NULL));
}


/*static*/ bool Window::IGuiMenu::create(const char *name, IGuiMenu **iGuiMenu)
{
    UniquePointer<GuiMenu> newMenu(new GuiMenu());
    if (!newMenu)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newMenu->initialize(name));

    *iGuiMenu = newMenu.release();

    return true;
}

GuiMenu::GuiMenu()
    : m_menu(NULL)
{
}

GuiMenu::GuiMenu(GtkWidget *widget)
    : GuiElementBase(widget)
{
}

GuiMenu::~GuiMenu()
{
    PROPAGATE_ERROR_CONTINUE(m_container.clear(m_menu));
}

bool GuiMenu::initialize(const char *name)
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    GtkWidget *menuItem = gtk_menu_item_new_with_label(name);
    if (!menuItem)
        ORIGINATE_ERROR("Out of memory");
    setWidget(menuItem);

    m_menu = gtk_menu_new();
    if (!m_menu)
        ORIGINATE_ERROR("Out of memory");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), GTK_WIDGET(m_menu));

    return true;
}

std::string GuiMenu::getName() const
{
    return std::string(gtk_menu_item_get_label(GTK_MENU_ITEM(getWidget())));
}


bool GuiMenu::add(Window::IGuiMenuItem *iGuiMenuItem)
{
    PROPAGATE_ERROR(m_container.add(m_menu, iGuiMenuItem));
    return true;
}

bool GuiMenu::remove(Window::IGuiMenuItem *iGuiMenuItem)
{
    PROPAGATE_ERROR(m_container.remove(getWidget(), iGuiMenuItem));
    return true;
}

/*static*/ bool Window::IGuiMenuBar::create(IGuiMenuBar **iGuiMenuBar)
{
    UniquePointer<GuiMenuBar> newMenuBar(new GuiMenuBar());
    if (!newMenuBar)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newMenuBar->initialize());

    *iGuiMenuBar = newMenuBar.release();

    return true;
}

GuiMenuBar::GuiMenuBar()
{
}

GuiMenuBar::GuiMenuBar(GtkWidget *widget)
    : GuiElementBase(widget)
{
}

GuiMenuBar::~GuiMenuBar()
{
    PROPAGATE_ERROR_CONTINUE(m_container.clear(getWidget()));
}

bool GuiMenuBar::initialize()
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    GtkWidget *menuBar = gtk_menu_bar_new();
    if (!menuBar)
        ORIGINATE_ERROR("Out of memory");
    setWidget(menuBar);

    return true;
}

bool GuiMenuBar::add(Window::IGuiMenu *iGuiMenu)
{
    PROPAGATE_ERROR(m_container.add(getWidget(), iGuiMenu));
    return true;
}

bool GuiMenuBar::remove(Window::IGuiMenu *iGuiMenu)
{
    PROPAGATE_ERROR(m_container.remove(getWidget(), iGuiMenu));
    return true;
}

Window::IGuiMenu *GuiMenuBar::getMenu(const char *name)
{
    for (GuiContainerBase::iterator it = m_container.begin(); it != m_container.end(); ++it)
    {
        GuiMenu *iGuiMenu = static_cast<GuiMenu*>(*it);
        if (iGuiMenu->getName() == std::string(name))
            return iGuiMenu;
    }

    return NULL;
}

GuiContainerBase::GuiContainerBase()
{
}

GuiContainerBase::~GuiContainerBase()
{
    // no elements should be left in the container else we will leak GtkWidgets
    assert(m_elements.empty());
}

bool GuiContainerBase::add(GtkWidget *widget, Window::IGuiElement *iGuiElement)
{
    GuiElement *guiElement = static_cast<GuiElement*>(iGuiElement);
    GtkWidget *widgetToAdd = guiElement->getWidget();

    gtk_container_add(GTK_CONTAINER(widget), widgetToAdd);
    gtk_widget_show_all(widgetToAdd);

    m_elements.push_back(iGuiElement);

    return true;
}

bool GuiContainerBase::attach(GtkWidget *widget, Window::IGuiElement *iGuiElement,
    unsigned int left, unsigned int top, unsigned int width, unsigned int height)
{
    if (!GTK_IS_GRID(widget))
        ORIGINATE_ERROR("Need to attach to a grid");

    GuiElement *guiElement = static_cast<GuiElement*>(iGuiElement);
    GtkWidget *widgetToAttach = guiElement->getWidget();

    gtk_grid_attach(GTK_GRID(widget), widgetToAttach, left, top, width, height);
    gtk_widget_show_all(widgetToAttach);

    m_elements.push_back(iGuiElement);

    return true;
}

bool GuiContainerBase::remove(GtkWidget *widget, Window::IGuiElement *iGuiElement)
{
    for (std::list<Window::IGuiElement*>::iterator it = m_elements.begin(); it != m_elements.end();
        ++it)
    {
        if (*it == iGuiElement)
        {
            GuiElement *element = static_cast<GuiElement*>(*it);
            gtk_container_remove(GTK_CONTAINER(widget), element->getWidget());
            m_elements.erase(it);
            return true;
        }
    }

    ORIGINATE_ERROR("Element is not part of the container");
    return false;
}

bool GuiContainerBase::clear(GtkWidget *widget)
{
    for (std::list<Window::IGuiElement*>::iterator it = m_elements.begin(); it != m_elements.end();
        ++it)
    {
        GuiElement *element = static_cast<GuiElement*>(*it);
        gtk_container_remove(GTK_CONTAINER(widget), element->getWidget());
        delete element;
    }
    m_elements.clear();

    return true;
}

GuiContainer::GuiContainer()
{
}

GuiContainer::GuiContainer(GtkWidget *widget)
    : GuiElementBase(widget)
{
}

GuiContainer::~GuiContainer()
{
    m_container.clear(getWidget());
}

bool GuiContainer::add(IGuiElement *iGuiElement)
{
    PROPAGATE_ERROR(m_container.add(getWidget(), iGuiElement));
    return true;
}

bool GuiContainer::remove(IGuiElement *iGuiElement)
{
    PROPAGATE_ERROR(m_container.remove(getWidget(), iGuiElement));
    return true;
}

/*static*/ bool Window::IGuiContainerGrid::create(IGuiContainerGrid **iGuiContainer)
{
    UniquePointer<GuiContainerGrid> newContainer(new GuiContainerGrid());
    if (!newContainer)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newContainer->initialize());

    *iGuiContainer = newContainer.release();

    return true;
}

GuiContainerGrid::GuiContainerGrid()
{
}

GuiContainerGrid::~GuiContainerGrid()
{
    m_container.clear(getWidget());
}

bool GuiContainerGrid::initialize()
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    GtkWidget *grid = gtk_grid_new();
    if (!grid)
        ORIGINATE_ERROR("Out of memory");
    setWidget(grid);

    // should expand horizontally
    gtk_widget_set_hexpand(grid, TRUE);

    return true;
}

bool GuiContainerGrid::add(IGuiElement *iGuiElement)
{
    PROPAGATE_ERROR(m_container.add(getWidget(), iGuiElement));
    return true;
}

bool GuiContainerGrid::remove(IGuiElement *iGuiElement)
{
    PROPAGATE_ERROR(m_container.remove(getWidget(), iGuiElement));
    return true;
}

bool GuiContainerGrid::attach(IGuiElement *iGuiElement, unsigned int left, unsigned int top,
    unsigned int width, unsigned int height)
{
    PROPAGATE_ERROR(m_container.attach(getWidget(), iGuiElement, left, top, width, height));
    return true;
}

/*static*/ bool Window::IGuiBuilder::create(const char *builderString, IGuiBuilder **iGuiBuilder)
{
    UniquePointer<GuiBuilder> newBuilder(new GuiBuilder(builderString));
    if (!newBuilder)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newBuilder->initialize());

    *iGuiBuilder = newBuilder.release();

    return true;
}

GuiBuilder::GuiBuilder(const char *builderString)
    : m_builderString(builderString)
    , m_builder(NULL)
{
}

GuiBuilder::~GuiBuilder()
{
    if (m_builder)
        g_object_unref(m_builder);
}

bool GuiBuilder::initialize()
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    // create the GUI element from the builder string
    m_builder = gtk_builder_new_from_string(m_builderString, -1);
    if (!m_builder)
        ORIGINATE_ERROR("Failed to build from string");

    // the first element is the root widget
    GSList *objList = gtk_builder_get_objects(m_builder);
    setWidget(GTK_WIDGET(objList->data));
    g_slist_free(objList);

    return true;
}

bool GuiBuilder::add(IGuiElement *iGuiElement)
{
    ORIGINATE_ERROR("Cant't add elements to a builder");
}

bool GuiBuilder::remove(IGuiElement *iGuiElement)
{
    ORIGINATE_ERROR("Cant't remove elements from a builder");
}

Window::IGuiElement *GuiBuilder::createElement(const char *name)
{
    IGuiElement *newElement = NULL;

    GObject *obj = gtk_builder_get_object(m_builder, name);
    if (obj)
    {
        GtkWidget *widget = GTK_WIDGET(obj);

        PROPAGATE_ERROR_CONTINUE(ArgusSamples::createElement(widget, &newElement));
    }

    return newElement;
}

/**
 * A GUI element for a value. The GUI element is part of a box with label and the actual
 * GUI element to adjust the value. Signals are connected to update the value when the GUI element
 * is changing and to update the GUI element when the value is changing.
 */
template<typename T> class GuiElementValue : public GuiElement, public IObserver
{
public:
    explicit GuiElementValue(Value<T> *value)
        : m_value(value)
    {
    }

    ~GuiElementValue()
    {
        PROPAGATE_ERROR_CONTINUE(cleanup());
    }

    enum Flags
    {
        FLAG_NONE         = (0 << 0),
        FLAG_FILE_CHOOSER = (1 << 0),       ///< this is a file chooser
        FLAG_PATH_CHOOSER = (1 << 1),       ///< this is a path chooser
        FLAG_COMBO_BOX_ENTRY = (1 << 2),    ///< the combo box has an entry
    };

    /**
     * Default initialize function, specializations handle the different types.
     */
    bool initialize(Flags flags = FLAG_NONE)
    {
        ORIGINATE_ERROR("Not implemented");
    }

    /**
     * Cleanup
     */
    bool cleanup()
    {
        ORIGINATE_ERROR("Not implemented");
    }

private:
    Value<T> *m_value;          //!< the value to modify

    /**
     * Call back triggered by GTK when the GUI element changes, default implementation,
     * specializations handle the different types.
     */
    static void onGuiElementChanged(GtkWidget *widget, gpointer data)
    {
        REPORT_ERROR("Not implemented (%s)", __FUNCTION__);
    }

    /**
     * Call back triggered by when the value changes, default implementation,
     * specializations handle the different types.
     */
    bool onValueChanged(const Observed &source)
    {
        ORIGINATE_ERROR("Not implemented (%s)", __FUNCTION__);
    }

    /**
     * Initialize elements needed for numeric input. This is commonly used for float, int, etc.
     * values.
     */
    bool initializeNumericInput();

    /**
     * Cleanup for numeric input.
     */
    bool cleanupNumericInput();

    /**
     * Call back triggered by GTK when the GUI element changes, numeric input implementation.
     */
    static void onGuiElementChangedNumericInput(GtkWidget *widget, gpointer data);

    /**
     * Call back triggered by when the value changes, numeric input implementation.
     */
    bool onValueChangedNumericInput(const Observed &source);

    /**
     * Call back triggered by when the validator changes, numeric input implementation.
     */
    bool onValidatorChangedNumericInput(const Observed &source);

    /**
     * Initialize elements needed for a combo box. This is commonly used for Argus::NamedUUID,
     * Argus::Size and enums.
     */
    bool initializeComboBox(Flags flags = FLAG_NONE);

    /**
     * Cleanup for a combo boxt.
     */
    bool cleanupComboBox();

    /**
     * Call back triggered by GTK when the GUI element changes, a combo box implementation.
     */
    static void onGuiElementChangedComboBox(GtkWidget *widget, gpointer data);

    /**
     * Call back triggered by when the value changes, a combo box implementation.
     */
    bool onValueChangedComboBox(const Observed &source);

    /**
     * Call back triggered by when the validator changes, a combo box implementation.
     */
    bool onValidatorChangedComboBox(const Observed &source);

    /**
     * Initialize elements needed for a range. This is used fo Argus::Range<T> values.
     */
    template <typename VT> bool initializeRange();

    /**
     * Cleanup for a range.
     */
    template <typename VT> bool cleanupRange();

    /**
     * Call back triggered by GTK when the GUI element changes, range implementation.
     */
    template <typename VT> static void onGuiElementChangedRange(GtkWidget *widget, gpointer data);

    /**
     * Call back triggered by when the value changes, range implementation.
     */
    template <typename VT> bool onValueChangedRange(const Observed &source);

    /**
     * Call back triggered by when the validator changes, range implementation.
     */
    template <typename VT> bool onValidatorChangedRange(const Observed &source);
};

/**
 * GuiElementValue onGuiElementChanged, bool specialization
 */
/*static*/ template<> void GuiElementValue<bool>::onGuiElementChanged(GtkWidget *widget,
    gpointer data)
{
    GuiElementValue *element = static_cast<GuiElementValue*>(data);
    GtkToggleButton *toggleButton = GTK_TOGGLE_BUTTON(widget);
    gboolean active = gtk_toggle_button_get_active(toggleButton);

    PROPAGATE_ERROR_CONTINUE(element->m_value->set(active ? true : false));
}

/**
 * GuiElementValue onValueChanged, bool specialization
 */
template<> bool GuiElementValue<bool>::onValueChanged(const Observed &source)
{
    assert(&source == m_value);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget()), m_value->get());
    return true;
}

/**
 * GuiElementValue initialize, bool specialization
 */
template<> bool GuiElementValue<bool>::initialize(Flags flags)
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    GtkWidget *checkButton = gtk_check_button_new();
    if (!checkButton)
        ORIGINATE_ERROR("Out of memory");
    setWidget(checkButton);

    // should expand horizontally
    gtk_widget_set_hexpand(checkButton, TRUE);

    // connect the widget with the call back function, the value is updated when the widget changes
    g_signal_connect(G_OBJECT(checkButton), "toggled", G_CALLBACK(onGuiElementChanged), this);

    // connect the value with the observer function, the widget is updated when the value changes
    PROPAGATE_ERROR(m_value->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<bool>::onValueChanged)));

    return true;
}

/**
 * GuiElementValue cleanup, bool specialization
 */
template<> bool GuiElementValue<bool>::cleanup()
{
    PROPAGATE_ERROR(m_value->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<bool>::onValueChanged)));
    return true;
}

/**
 * onGuiElementChanged for numeric inputs
 */
/* static */ template<typename T> void GuiElementValue<T>::onGuiElementChangedNumericInput(
    GtkWidget *widget, gpointer data)
{
    GuiElementValue *element = static_cast<GuiElementValue*>(data);
    GtkSpinButton *spinButton = GTK_SPIN_BUTTON(widget);
    double value = gtk_spin_button_get_value(spinButton);

    PROPAGATE_ERROR_CONTINUE(element->m_value->set(static_cast<T>(value)));
}

/**
 * onValueChanged for numeric inputs
 */
template<typename T> bool GuiElementValue<T>::onValueChangedNumericInput(const Observed &source)
{
    assert(&source == m_value);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget()), static_cast<double>(m_value->get()));
    return true;
}

/**
 * onValidatorChanged for numeric inputs
 */
template<typename T> bool GuiElementValue<T>::onValidatorChangedNumericInput(const Observed &source)
{
    assert(&source == (m_value->getValidator()));

    IValidator<T> *validator = m_value->getValidator();
    T min = std::numeric_limits<T>::min();
    T max = std::numeric_limits<T>::max();
    if (validator)
    {
        validator->getMin(&min);
        validator->getMax(&max);
    }

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(getWidget()), min, max);
    return true;
}

/**
 * Initialize a GUI element to input numeric values
 */
template<typename T> bool GuiElementValue<T>::initializeNumericInput()
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    // create the spin button
    GtkWidget *spinButton = gtk_spin_button_new_with_range(std::numeric_limits<T>::min(),
        std::numeric_limits<T>::max(), 1);
    if (!spinButton)
        ORIGINATE_ERROR("Out of memory");
    setWidget(spinButton);

    // should expand horizontally
    gtk_widget_set_hexpand(spinButton, TRUE);
    // set numeric for integers and digits for floats
    if (std::numeric_limits<T>::is_integer)
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinButton), TRUE);
    else
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinButton), 3);

    // connect the widget with the call back function, the value is updated when the widget changes
    g_signal_connect(G_OBJECT(spinButton), "value-changed",
        G_CALLBACK(GuiElementValue<T>::onGuiElementChangedNumericInput), this);

    // connect the value with the observer function, the widget is updated when the value changes
    PROPAGATE_ERROR(m_value->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValueChangedNumericInput)));

    // connect the validator with the observer function, the widget is updated when the validator
    // changes
    PROPAGATE_ERROR(m_value->getValidator()->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(
            &GuiElementValue<T>::onValidatorChangedNumericInput)));

    return true;
}

/**
 * cleanup for numeric values
 */
template<typename T> bool GuiElementValue<T>::cleanupNumericInput()
{
    PROPAGATE_ERROR(m_value->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValueChangedNumericInput)));
    return true;
}

/**
 * GuiElementValue initialize, int32_t specialization
 */
template<> bool GuiElementValue<int32_t>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeNumericInput());
    return true;
}

/**
 * GuiElementValue cleanup, int32_t specialization
 */
template<> bool GuiElementValue<int32_t>::cleanup()
{
    PROPAGATE_ERROR(cleanupNumericInput());
    return true;
}

/**
 * GuiElementValue initialize, uint32_t specialization
 */
template<> bool GuiElementValue<uint32_t>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeNumericInput());
    return true;
}

/**
 * GuiElementValue cleanup, int32_t specialization
 */
template<> bool GuiElementValue<uint32_t>::cleanup()
{
    PROPAGATE_ERROR(cleanupNumericInput());
    return true;
}

/**
 * GuiElementValue initialize, float specialization
 */
template<> bool GuiElementValue<float>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeNumericInput());
    return true;
}

/**
 * GuiElementValue cleanup, float specialization
 */
template<> bool GuiElementValue<float>::cleanup()
{
    PROPAGATE_ERROR(cleanupNumericInput());
    return true;
}

/**
 * GuiElementValue onGuiElementChanged, std::string specialization
 */
/*static*/ template<> void GuiElementValue<std::string>::onGuiElementChanged(GtkWidget *widget,
    gpointer data)
{
    GuiElementValue *element = static_cast<GuiElementValue*>(data);
    const gchar *text;

    if (GTK_IS_FILE_CHOOSER_BUTTON(widget))
    {
        GtkFileChooser *fileChooser = GTK_FILE_CHOOSER(widget);
        text = gtk_file_chooser_get_filename(fileChooser);
    }
    else
    {
        GtkEntry *entry = GTK_ENTRY(widget);
        text = gtk_entry_get_text(entry);
    }

    PROPAGATE_ERROR_CONTINUE(element->m_value->set(text));
}

/**
 * GuiElementValue onValueChanged, std::string specialization
 */
template<> bool GuiElementValue<std::string>::onValueChanged(const Observed &source)
{
    assert(&source == m_value);
    if (GTK_IS_FILE_CHOOSER_BUTTON(getWidget()))
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(getWidget()),
            m_value->get().c_str());
    }
    else
    {
        gtk_entry_set_text(GTK_ENTRY(getWidget()), m_value->get().c_str());
    }
    return true;
}

/**
 * GuiElementValue initialize, std::string specialization
 */
template<> bool GuiElementValue<std::string>::initialize(Flags flags)
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    if ((flags & FLAG_FILE_CHOOSER) || (flags & FLAG_PATH_CHOOSER))
    {
        GtkWidget *fileChooserButton;

        if (flags & FLAG_PATH_CHOOSER)
        {
            fileChooserButton = gtk_file_chooser_button_new("Select a folder",
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        }
        else
        {
            fileChooserButton = gtk_file_chooser_button_new("Select a file",
                GTK_FILE_CHOOSER_ACTION_OPEN);
        }
        if (!fileChooserButton)
            ORIGINATE_ERROR("Out of memory");
        setWidget(fileChooserButton);

        // should expand horizontally
        gtk_widget_set_hexpand(fileChooserButton, TRUE);

        // connect the widget with the call back function, the value is updated when the widget
        // changes
        g_signal_connect(G_OBJECT(fileChooserButton), "file-set", G_CALLBACK(onGuiElementChanged),
            this);
    }
    else
    {
        GtkWidget *entry = gtk_entry_new();
        if (!entry)
            ORIGINATE_ERROR("Out of memory");
        setWidget(entry);

        // should expand horizontally
        gtk_widget_set_hexpand(entry, TRUE);

        // connect the widget with the call back function, the value is updated when the widget
        // changes
        g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(onGuiElementChanged), this);
    }

    // connect the value with the observer function, the widget is updated when the value changes
    PROPAGATE_ERROR(m_value->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<std::string>::onValueChanged)));

    return true;
}

/**
 * GuiElementValue cleanup, std::string specialization
 */
template<> bool GuiElementValue<std::string>::cleanup()
{
    PROPAGATE_ERROR(m_value->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<std::string>::onValueChanged)));
    return true;
}

/**
 * onGuiElementChanged for a combo box
 */
template<typename T> void GuiElementValue<T>::onGuiElementChangedComboBox(GtkWidget *widget,
    gpointer data)
{
    GuiElementValue *element = static_cast<GuiElementValue*>(data);
    GtkComboBoxText *comboBoxText = GTK_COMBO_BOX_TEXT(widget);
    const gchar *text = gtk_combo_box_text_get_active_text(comboBoxText);
    if (text)
        PROPAGATE_ERROR_CONTINUE(element->m_value->setFromString(text));
}

/**
 * onValueChanged for a combo box
 */
template<typename T> bool GuiElementValue<T>::onValueChangedComboBox(const Observed &source)
{
    assert(&source == m_value);

    const T value = m_value->get();

    IValidator<T> *validator = m_value->getValidator();
    if (!validator)
        ORIGINATE_ERROR("Value needs to have a validator");

    const std::vector<T> *values;
    PROPAGATE_ERROR(validator->getValidValues(&values));

    for (size_t index = 0; index < values->size(); ++index)
    {
        if ((*values)[index] == value)
        {
            gtk_combo_box_set_active(GTK_COMBO_BOX(getWidget()), index);
            return true;
        }
    }

    // if we get here the value is valid, that means this is a value allowing arbitrary values
    // additional to the fixed ones. Check if the combo box has an entry widget, and pass the value
    // to this.
    if (gtk_combo_box_get_has_entry(GTK_COMBO_BOX(getWidget())))
    {
        GtkWidget *entry = gtk_bin_get_child(GTK_BIN(getWidget()));
        assert(entry && GTK_IS_ENTRY(entry));
        gtk_entry_set_text(GTK_ENTRY(entry), validator->toString(value).c_str());
        return true;
    }

    ORIGINATE_ERROR("Value not found");
}

/**
 * onValidatorChangedComboBox for a combo box
 */
template<typename T> bool GuiElementValue<T>::onValidatorChangedComboBox(const Observed &source)
{
    assert(&source == (m_value->getValidator()));

    // get the validator and the entries
    IValidator<T> *validator = m_value->getValidator();
    if (!validator)
        ORIGINATE_ERROR("Value needs to have a validator");

    const std::vector<T> *values;
    PROPAGATE_ERROR(validator->getValidValues(&values));

    // remove all present entries
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(getWidget()));

    // add entries from the validator to the combo box text widget
    for (size_t index = 0; index < values->size(); ++index)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(getWidget()),
            validator->toString((*values)[index]).c_str());
    }

    return true;
}

/**
 * Initialize a GUI element to input a combo box
 */
template<typename T> bool GuiElementValue<T>::initializeComboBox(Flags flags)
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    GtkWidget *comboBoxText;
    if (flags & FLAG_COMBO_BOX_ENTRY)
    {
        comboBoxText = gtk_combo_box_text_new_with_entry();
    }
    else
    {
        comboBoxText = gtk_combo_box_text_new();
    }
    if (!comboBoxText)
        ORIGINATE_ERROR("Out of memory");
    setWidget(comboBoxText);

    // should expand horizontally
    gtk_widget_set_hexpand(comboBoxText, TRUE);

    // connect the widget with the call back function, the value is updated when the widget changes
    g_signal_connect(G_OBJECT(comboBoxText), "changed",
        G_CALLBACK(GuiElementValue<T>::onGuiElementChangedComboBox), this);

    // connect the validator with the observer function, the widget is updated when the validator
    // changes
    PROPAGATE_ERROR(m_value->getValidator()->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValidatorChangedComboBox)));

    // connect the value with the observer function, the widget is updated when the value changes
    PROPAGATE_ERROR(m_value->registerObserver(this, static_cast<IObserver::CallbackFunction>(
        &GuiElementValue<T>::onValueChangedComboBox)));

    return true;
}

/**
 * cleanup for a combo box
 */
template<typename T> bool GuiElementValue<T>::cleanupComboBox()
{
    PROPAGATE_ERROR(m_value->getValidator()->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValidatorChangedComboBox)));
    PROPAGATE_ERROR(m_value->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValueChangedComboBox)));
    return true;
}

/**
 * GuiElementValue initialize, 'Argus::NamedUUID' specialization
 */
template<> bool GuiElementValue<Argus::NamedUUID>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeComboBox());
    return true;
}

/**
 * GuiElementValue cleanup, 'Argus::NamedUUID' specialization
 */
template<> bool GuiElementValue<Argus::NamedUUID>::cleanup()
{
    PROPAGATE_ERROR(cleanupComboBox());
    return true;
}

/**
 * onGuiElementChanged for a range
 */
/* static */ template<typename T> template<typename VT> void
    GuiElementValue<T>::onGuiElementChangedRange(GtkWidget *widget, gpointer data)
{
    GuiElementValue *element = static_cast<GuiElementValue*>(data);
    GtkSpinButton *spinButton = GTK_SPIN_BUTTON(widget);
    VT value = static_cast<VT>(gtk_spin_button_get_value(spinButton));

    T range = element->m_value->get();

    gpointer index = g_object_get_data(G_OBJECT(widget), "index");
    if (index == reinterpret_cast<gpointer>(0))
    {
        // make sure that the range is value (min <= max)
        if (value > range.max)
        {
            value = range.max;
            gtk_spin_button_set_value(spinButton, static_cast<double>(value));
        }
        range.min = value;
    }
    else
    {
        assert(index == reinterpret_cast<gpointer>(1));

        // make sure that the range is value (min <= max)
        if (value < range.min)
        {
            value = range.min;
            gtk_spin_button_set_value(spinButton, static_cast<double>(value));
        }
        range.max = value;
    }

    PROPAGATE_ERROR_CONTINUE(element->m_value->set(range));
}

// Instantiation, usually they should already have been instantiated because they are used below,
// but for some reason they are not, maybe because they are static.
/* static */ template void
    GuiElementValue<Argus::Range<uint64_t> >::onGuiElementChangedRange<uint64_t>(
        GtkWidget *widget, gpointer data);
/* static */ template void
    GuiElementValue<Argus::Range<float> >::onGuiElementChangedRange<float>(
        GtkWidget *widget, gpointer data);

/**
 * onValueChanged for a range
 */
template<typename T> template<typename VT> bool GuiElementValue<T>::onValueChangedRange(
    const Observed &source)
{
    assert(&source == m_value);
    T range = m_value->get();

    GList *children = gtk_container_get_children(GTK_CONTAINER(getWidget()));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children->data), static_cast<double>(range.min));
    children = g_list_next(children);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children->data), static_cast<double>(range.max));

    g_list_free(children);

    return true;
}

/**
 * onValidatorChanged for numeric inputs
 */
template<typename T> template<typename VT> bool GuiElementValue<T>::onValidatorChangedRange(
    const Observed &source)
{
    assert(&source == (m_value->getValidator()));

    // get the validator and the min and max values
    IValidator<T> *validator = m_value->getValidator();
    T min(std::numeric_limits<VT>::min(), std::numeric_limits<VT>::max());
    T max(std::numeric_limits<VT>::min(), std::numeric_limits<VT>::max());
    if (validator)
    {
        validator->getMin(&min);
        validator->getMax(&max);
    }

    // Also get the value, if the range of a spin button is set it automatically updates
    // the spin button value to be inside the range. But this would modify m_value.
    T range = m_value->get();

    GList *children = gtk_container_get_children(GTK_CONTAINER(getWidget()));

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(children->data), min.min, min.max);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children->data), static_cast<double>(range.min));

    children = g_list_next(children);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(children->data), max.min, max.max);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children->data), static_cast<double>(range.max));

    g_list_free(children);

    return true;
}

/**
 * Initialize a GUI element to input a range
 */
template<typename T> template<typename VT> bool GuiElementValue<T>::initializeRange()
{
    if (getWidget())
        ORIGINATE_ERROR("Already initialized");

    // create a box, two spin buttons will be added
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (!box)
        ORIGINATE_ERROR("Out of memory");
    setWidget(box);

    // should expand horizontally
    gtk_widget_set_hexpand(box, TRUE);

    // create min spin button
    GtkWidget *spinButton = gtk_spin_button_new_with_range(std::numeric_limits<VT>::min(),
        std::numeric_limits<VT>::max(), 1);
    if (!spinButton)
        ORIGINATE_ERROR("Out of memory");

    // add to box
    gtk_box_pack_start(GTK_BOX(box), spinButton, TRUE, TRUE, 0);

    // should expand horizontally
    gtk_widget_set_hexpand(spinButton, TRUE);
    // set numeric for integers
    // set numeric for integers and digits for floats
    if (std::numeric_limits<T>::is_integer)
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinButton), TRUE);
    else
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinButton), 3);

    // index 0 is the min value
    g_object_set_data(G_OBJECT(spinButton), "index", reinterpret_cast<gpointer>(0));
    // connect the widget with the call back function, the value is updated when the widget changes
    g_signal_connect(G_OBJECT(spinButton), "value-changed",
        G_CALLBACK(GuiElementValue<T>::onGuiElementChangedRange<VT>), this);

    // create max spin button
    spinButton = gtk_spin_button_new_with_range(std::numeric_limits<VT>::min(),
        std::numeric_limits<VT>::max(), 1);
    if (!spinButton)
        ORIGINATE_ERROR("Out of memory");

    // add to box
    gtk_box_pack_start(GTK_BOX(box), spinButton, TRUE, TRUE, 0);

    // should expand horizontally
    gtk_widget_set_hexpand(spinButton, TRUE);
    // set numeric for integers
    // set numeric for integers and digits for floats
    if (std::numeric_limits<T>::is_integer)
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinButton), TRUE);
    else
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinButton), 3);

    // index 1 is the max value
    g_object_set_data(G_OBJECT(spinButton), "index", reinterpret_cast<gpointer>(1));
    // connect the widget with the call back function, the value is updated when the widget changes
    g_signal_connect(G_OBJECT(spinButton), "value-changed",
        G_CALLBACK(GuiElementValue<T>::onGuiElementChangedRange<VT>), this);

    // connect the validator with the observer function, the widget is updated when the validator
    // changes
    PROPAGATE_ERROR(m_value->getValidator()->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(
            &GuiElementValue<T>::onValidatorChangedRange<VT>)));

    // connect the value with the observer function, the widget is updated when the value changes
    PROPAGATE_ERROR(m_value->registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValueChangedRange<VT>)));

    return true;
}

/**
 * cleanup for a range
 */
template<typename T> template<typename VT> bool GuiElementValue<T>::cleanupRange()
{
    PROPAGATE_ERROR(m_value->unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&GuiElementValue<T>::onValueChangedRange<VT>)));
    return true;
}

/**
 * GuiElementValue initialize, 'Argus::Range<uint64_t>' specialization
 */
template<> bool GuiElementValue<Argus::Range<uint64_t> >::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeRange<uint64_t>());
    return true;
}

/**
 * GuiElementValue cleanup, 'Argus::Range<uint64_t>' specialization
 */
template<> bool GuiElementValue<Argus::Range<uint64_t> >::cleanup()
{
    PROPAGATE_ERROR(cleanupRange<uint64_t>());
    return true;
}

/**
 * GuiElementValue initialize, 'Argus::Range<float>' specialization
 */
template<> bool GuiElementValue<Argus::Range<float> >::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeRange<float>());
    return true;
}

/**
 * GuiElementValue cleanup, 'Argus::Range<float>' specialization
 */
template<> bool GuiElementValue<Argus::Range<float> >::cleanup()
{
    PROPAGATE_ERROR(cleanupRange<float>());
    return true;
}

/**
 * GuiElementValue initialize, 'Argus::Size' specialization
 */
template<> bool GuiElementValue<Argus::Size>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeComboBox(FLAG_COMBO_BOX_ENTRY));
    return true;
}

/**
 * GuiElementValue cleanup, 'Argus::Size' specialization
 */
template<> bool GuiElementValue<Argus::Size>::cleanup()
{
    PROPAGATE_ERROR(cleanupComboBox());
    return true;
}

/**
 * GuiElementValue initialize, 'ValueTypeEnum' specialization
 */
template<> bool GuiElementValue<Window::IGuiElement::ValueTypeEnum>::initialize(Flags flags)
{
    PROPAGATE_ERROR(initializeComboBox());
    return true;
}

/**
 * GuiElementValue cleanup, 'ValueTypeEnum' specialization
 */
template<> bool GuiElementValue<Window::IGuiElement::ValueTypeEnum>::cleanup()
{
    PROPAGATE_ERROR(cleanupComboBox());
    return true;
}

/**
 * Create a GuiElement for a value
 */
template<typename T> /*static*/ bool Window::IGuiElement::createValue(Value<T> *value,
    IGuiElement **element)
{
    UniquePointer<GuiElementValue<T> > newGuiElement(new GuiElementValue<T>(value));
    if (!newGuiElement)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newGuiElement->initialize());

    *element = newGuiElement.release();
    return true;
}

// Instantiation to have them available at link time.
template bool Window::IGuiElement::createValue<bool>(Value<bool> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<float>(Value<float> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<int32_t>(Value<int32_t> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<uint32_t>(Value<uint32_t> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<std::string>(Value<std::string> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<Argus::NamedUUID>(Value<Argus::NamedUUID> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<Argus::Size>(Value<Argus::Size> *value,
    IGuiElement **element);
template bool Window::IGuiElement::createValue<Argus::Range<uint64_t> >(
    Value<Argus::Range<uint64_t> > *value, IGuiElement **element);
template bool Window::IGuiElement::createValue<Argus::Range<float> >(
    Value<Argus::Range<float> > *value, IGuiElement **element);
template bool Window::IGuiElement::createValue<Window::IGuiElement::ValueTypeEnum>(
    Value<ValueTypeEnum> *value, IGuiElement **element);

/**
 * Create a GUI element for a file chooser.
 */
/*static*/ bool Window::IGuiElement::createFileChooser(Value<std::string> *value, bool pathsOnly,
    Window::IGuiElement **element)
{
    UniquePointer<GuiElementValue<std::string> > newGuiElement(
        new GuiElementValue<std::string>(value));
    if (!newGuiElement)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newGuiElement->initialize(
        pathsOnly ?
            GuiElementValue<std::string>::FLAG_PATH_CHOOSER :
            GuiElementValue<std::string>::FLAG_FILE_CHOOSER));

    *element = newGuiElement.release();
    return true;
}

/**
 * Create a GUI element for a label
 */
/*static*/ bool Window::IGuiElement::createLabel(const char *labelText, IGuiElement **element)
{
    UniqueGObject<GtkWidget> label(gtk_label_new(labelText));
    if (!label)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(createElement(label.get(), element));
    label.release();

    return true;
}

/**
 * A GUI to execute an action
 */
class GuiElementAction : public GuiElement
{
public:
    explicit GuiElementAction(const char *name, GuiActionCallBackFunc function,
        void *userPtr)
        : m_name(name)
        , m_function(function)
        , m_userPtr(userPtr)
    {
    }
    ~GuiElementAction()
    {
    }

    bool initialize(Flags flags, Icon icon)
    {
        if (getWidget())
            ORIGINATE_ERROR("Already initialized");

        GtkWidget *button = NULL;
        if (flags & FLAG_BUTTON_TOGGLE)
        {
            button = gtk_toggle_button_new();
            if (!button)
                ORIGINATE_ERROR("Out of memory");

            gtk_widget_add_events(button, GDK_BUTTON_PRESS_MASK);
            g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(onButtonToggled), this);
        }
        else
        {
            button = gtk_button_new();
            if (!button)
                ORIGINATE_ERROR("Out of memory");

            gtk_widget_add_events(button, GDK_BUTTON_PRESS_MASK);
            g_signal_connect(G_OBJECT(button), "button-press-event", G_CALLBACK(onButtonPressEvent),
                this);
        }

        setWidget(button);

        // should expand horizontally
        gtk_widget_set_hexpand(button, TRUE);

        if (icon == ICON_NONE)
        {
            gtk_button_set_label(GTK_BUTTON(button), m_name.c_str());
        }
        else
        {
            const char *iconName = NULL;

            switch (icon)
            {
            case ICON_PREVIOUS:
                iconName = "go-previous";
                break;
            case ICON_NEXT:
                iconName = "go-next";
                break;
            case ICON_MEDIA_REWIND:
                iconName = "media-seek-backward";
                break;
            case ICON_MEDIA_PLAY:
                iconName = "media-playback-start";
                break;
            case ICON_MEDIA_RECORD:
                iconName = "media-record";
                break;
            default:
                ORIGINATE_ERROR("Unhandled icon");
            }

            UniqueGObject<GtkWidget> image(gtk_image_new_from_icon_name(iconName,
                GTK_ICON_SIZE_BUTTON));
            if (!image)
                ORIGINATE_ERROR("Out of memory");

            gtk_button_set_image(GTK_BUTTON(button), image.release());

            gtk_widget_set_tooltip_text(button, m_name.c_str());
        }

        return true;
    }

private:
    std::string m_name;                 //!< action name
    GuiActionCallBackFunc m_function;   //!< call back function
    void *m_userPtr;

    static void onButtonPressEvent(GtkWidget *widget, GdkEventButton *event, gpointer data)
    {
        GuiElementAction *element = static_cast<GuiElementAction*>(data);
        PROPAGATE_ERROR_CONTINUE(element->m_function(element->m_userPtr, NULL));
    }
    static void onButtonToggled(GtkToggleButton *togglebutton, gpointer data)
    {
        GuiElementAction *element = static_cast<GuiElementAction*>(data);
        PROPAGATE_ERROR_CONTINUE(element->m_function(element->m_userPtr, NULL));
    }
};

/**
 * Create a GUI element to execute an action.
 */
/*static*/ bool Window::IGuiElement::createAction(const char *name, GuiActionCallBackFunc function,
    void *userPtr, Flags flags, Icon icon, IGuiElement **element)
{
    UniquePointer<GuiElementAction> newGuiElement(new GuiElementAction(name, function, userPtr));
    if (!newGuiElement)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(newGuiElement->initialize(flags, icon));

    *element = newGuiElement.release();
    return true;
}

}; // namespace ArgusSamples
