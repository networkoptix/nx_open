import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { NxLanguageDropdown }        from "../dropdowns/language/language.component";
import { NxAccountSettingsDropdown } from "../dropdowns/account-settings/account-settings.component";

@NgModule({
    imports: [
        CommonModule
    ],
    declarations: [
        // NxLanguageDropdown,
        // NxAccountSettingsDropdown
    ],
    entryComponents: [
        // NxLanguageDropdown,
        // NxAccountSettingsDropdown
    ],
    exports: [
        // NxLanguageDropdown,
        // NxAccountSettingsDropdown
    ]
})
export class DropdownsModule {
}
