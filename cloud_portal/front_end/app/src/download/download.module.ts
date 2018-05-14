import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { DownloadComponent } from './download.component';
import { OsResolver }        from "../core";

const appRoutes: Routes = [
    // {path: 'downloads', component: DownloadComponent},
    {path: 'download', component: DownloadComponent, resolve: {platform: OsResolver}},
    {path: 'download/:platform', component: DownloadComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [
        OsResolver
    ],
    declarations: [
        DownloadComponent,
    ],
    bootstrap: []
})
export class DownloadModule {
}
