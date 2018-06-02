import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';


import { NonSupportedBrowserComponent } from './non-supported-browser.component';

const appRoutes: Routes = [
    {path: 'browser', component: NonSupportedBrowserComponent},
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [],
    declarations: [
        NonSupportedBrowserComponent,
    ],
    bootstrap: []
})
export class NonSupportedBrowserModule {
}
