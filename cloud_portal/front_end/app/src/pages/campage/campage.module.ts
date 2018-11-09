import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule }          from '@angular/forms';
import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { TranslateModule }      from '@ngx-translate/core';

import { NxCampageComponent } from './campage.component';
import { ComponentsModule }   from '../../components/components.module';
import { CamTableComponent } from './temp_components/cam-table/cam-table.component';

const appRoutes: Routes = [
    { path: 'campage', component: NxCampageComponent }
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
        NxCampageComponent,
        CamTableComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxCampageComponent
    ],
    exports        : [
        NxCampageComponent
    ]
})

export class CampageModule { }
