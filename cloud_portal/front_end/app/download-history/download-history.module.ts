import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { ReleaseComponent } from './release/release.component';
import { DownloadHistoryComponent } from './download-history.component';

const appRoutes: Routes = [
    {path: 'downloads/history', component: DownloadHistoryComponent},
    {path: 'downloads/:build', component: DownloadHistoryComponent}
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
    ],
    declarations: [
        DownloadHistoryComponent,
        ReleaseComponent
    ],
    bootstrap: []
})
export class DownloadHistoryModule {
}
