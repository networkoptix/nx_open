import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';

import { NxHeaderComponent } from './header.component';

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule
    ],
    providers: [
    ],
    declarations: [
        NxHeaderComponent
    ],
    bootstrap: []
})
export class NxHeaderModule {
}
