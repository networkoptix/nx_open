import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxUsersDetailComponent } from './users.component';

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
        NxUsersDetailComponent
    ],
    bootstrap: [],
    entryComponents: [
        NxUsersDetailComponent
    ],
    exports: [
        NxUsersDetailComponent
    ]
})
export class UsersDetailModule {
}
