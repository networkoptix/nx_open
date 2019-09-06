import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxOverviewComponent } from './overview.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../../../components/components.module';

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
        NxOverviewComponent
    ],
    bootstrap: [],
    entryComponents: [
        NxOverviewComponent
    ],
    exports: [
        NxOverviewComponent
    ]
})
export class NxOverviewModule {
}
