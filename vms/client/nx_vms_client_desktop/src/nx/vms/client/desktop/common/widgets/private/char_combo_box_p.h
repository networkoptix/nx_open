// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_widgets/combo_box_list_view.h>

namespace nx::vms::client::desktop {

// -------------------------------------------------------------------------- //
// CharComboBoxListView
// -------------------------------------------------------------------------- //
class CharComboBoxListView: public ComboBoxListView {
    Q_OBJECT
    typedef ComboBoxListView base_type;

public:
    CharComboBoxListView(QComboBox *comboBox = nullptr): ComboBoxListView(comboBox) {}

    virtual void keyboardSearch(const QString &search) override {
        if(!search.isEmpty()) {
            QModelIndex currentIndex = this->currentIndex();
            base_type::keyboardSearch(QString()); /* Clear search data. */
            setCurrentIndex(currentIndex);
        }

        base_type::keyboardSearch(search);
    }
};

} // namespace nx::vms::client::desktop
