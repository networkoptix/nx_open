import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxOtherDetailsComponent } from './others.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../../components/components.module';

// TODO: Remove it after test

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        TranslateModule,
        ComponentsModule
    ],
    providers: [],
    declarations: [
        NxOtherDetailsComponent
    ],
    bootstrap: [],
    entryComponents: [
        NxOtherDetailsComponent
    ],
    exports: [
        NxOtherDetailsComponent
    ]
})
export class OtherDetailsModule {
}
