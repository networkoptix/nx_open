import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import {FormsModule, ReactiveFormsModule } from '@angular/forms';

import { NgbModalModule, NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

import { QuoteService } from "../../app/core";

// import { LoginModalContent, NxModalLoginComponent} from "../dialogs/login/login.component";
// import { NxProcessButtonComponent } from "../components/process-button/process-button.component";

import { BarComponent } from './bar.component';
// import { DropdownsModule} from '../dropdowns/dropdowns.module';
import {NxAccountSettingsDropdown} from "../dropdowns/account-settings/account-settings.component";
import { NxLanguageDropdown } from "../dropdowns/language/language.component";


const appRoutes: Routes = [
    { path: 'bar', component: BarComponent }
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
        // DropdownsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [
        // cloudApiService,
        QuoteService,
        // uuid2Service,
        // languageService
    ],
    declarations: [
        BarComponent,
        NxLanguageDropdown,
        NxAccountSettingsDropdown
    ],
    bootstrap: []
})
export class BarModule {
}
