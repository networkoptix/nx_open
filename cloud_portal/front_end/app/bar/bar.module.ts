import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import {FormsModule, ReactiveFormsModule } from '@angular/forms';

import { NgbModalModule, NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

import { QuoteService } from "../../app/core";

import { NxLanguageDropdown } from "../dropdown/language.component";
import { LoginModalContent, NxModalLoginComponent} from "../dialogs/login/login.component";
import { NxProcessButtonComponent } from "../components/process-button/process-button.component";

import { BarComponent } from './bar.component';

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
        NxModalLoginComponent,
        LoginModalContent,
        NxProcessButtonComponent
    ],
    bootstrap: []
})
export class BarModule {
}
