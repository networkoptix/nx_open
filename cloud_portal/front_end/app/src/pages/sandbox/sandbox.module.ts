import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule }          from '@angular/forms';
import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { TranslateModule }      from '@ngx-translate/core';

import { NxSandboxComponent } from './sandbox.component';
import { ComponentsModule }   from '../../components/components.module';

const appRoutes: Routes = [
    { path: 'sandbox', component: NxSandboxComponent }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        FormsModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxSandboxComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxSandboxComponent
    ],
    exports        : [
        NxSandboxComponent
    ]
})
export class SandboxModule {
}
