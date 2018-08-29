import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxMainComponent } from './main.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../components/components.module';


const appRoutes: Routes = [
    {path: 'main', component: NxMainComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [],
    declarations: [
        NxMainComponent,
    ],
    bootstrap: [],
    entryComponents: [
        NxMainComponent
    ],
    exports: [
        NxMainComponent
    ]
})
export class MainModule {
}
