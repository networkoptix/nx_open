import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { DownloadComponent } from './download.component';


const appRoutes: Routes = [
    {path: 'downloads', component: DownloadComponent},
    {path: 'download', component: DownloadComponent},
    {path: 'download/:platform', component: DownloadComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        // NgbTab,
        // NgbTabset,
        // NgbTabContent,

        RouterModule.forChild(appRoutes)
    ],
    providers: [
        // NgbTab,
        // NgbTabset,
        // NgbTabContent,
    ],
    declarations: [
        DownloadComponent,
    ],
    bootstrap: []
})
export class DownloadModule {
}
