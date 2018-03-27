import { NgModule }                         from '@angular/core';
import { CommonModule }                     from '@angular/common';
import { BrowserModule }                    from '@angular/platform-browser';
import { UpgradeModule }                    from '@angular/upgrade/static';
import { RouterModule, Routes }             from '@angular/router';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';

import { NgbModalModule, NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

import { QuoteService } from "../../app/core";

import { BarComponent }              from './bar.component';
import { NxAccountSettingsDropdown } from "../dropdowns/account-settings/account-settings.component";
import { NxLanguageDropdown }        from "../dropdowns/language/language.component";
import { NxActiveSystemDropdown }    from "../dropdowns/active-system/active-system.component";
import { NxSystemsDropdown }         from "../dropdowns/systems/systems.component";


const appRoutes: Routes = [
    {path: 'bar', component: BarComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbDropdownModule,
        NgbModalModule,
        FormsModule,
        ReactiveFormsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [
        QuoteService,
    ],
    declarations: [
        BarComponent,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown
    ],
    bootstrap: []
})
export class BarModule {
}
