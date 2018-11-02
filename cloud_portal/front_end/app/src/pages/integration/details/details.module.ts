import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxIntegrationDetailsComponent } from './details.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../../components/components.module';

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        RouterModule,
        NgbModule,
        TranslateModule,
        ComponentsModule
    ],
    providers: [],
    declarations: [
        NxIntegrationDetailsComponent
    ],
    bootstrap: [],
    entryComponents: [
        NxIntegrationDetailsComponent
    ],
    exports: [
        NxIntegrationDetailsComponent
    ]
})
export class IntegrationDetailModule {
}
