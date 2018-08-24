import { Injectable, NgModule }                  from '@angular/core';
import { CommonModule }                          from '@angular/common';
import { BrowserModule }                         from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule }     from '@angular/upgrade/static';
import { Router, Resolve, RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxSandboxComponent }           from './sandbox.component';
import { Observable, EMPTY as empty }  from 'rxjs';
import { DeviceDetectorService }       from 'ngx-device-detector';
import { FormsModule, EmailValidator } from '@angular/forms';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../components/components.module';


const appRoutes: Routes = [
    {path: 'sandbox', component: NxSandboxComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        FormsModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [
    ],
    declarations: [
        NxSandboxComponent,
    ],
    bootstrap: [],
    entryComponents: [
        NxSandboxComponent
    ],
    exports: [
        NxSandboxComponent
    ]
})
export class SandboxModule {
}
