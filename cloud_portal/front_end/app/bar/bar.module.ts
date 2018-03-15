import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';
import { NgbModalModule } from '@ng-bootstrap/ng-bootstrap';

// import { cloudApiService } from "../scripts/services/cloud_api";
// import { uuid2Service } from '../../app/scripts/services/angular-uuid2';
import { QuoteService } from "../../app/core";
// import { languageService } from "../scripts/services/language";

import { NxLanguageDropdown } from "../dropdown/language.component";
import { NxModalComponent } from "../modal/modal.component";

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
        NxModalComponent],
    bootstrap: []
})
export class BarModule {
}
