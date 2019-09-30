import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule, ReactiveFormsModule }          from '@angular/forms';
import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { TranslateModule }      from '@ngx-translate/core';
import { Angular2CsvModule }    from 'angular2-csv';

import { NxIpvdComponent }    from './ipvd.component';
import { ComponentsModule }   from '../../components/components.module';
import { CamTableComponent }  from './cam-components/cam-table/cam-table.component';
import { CamViewComponent }   from './cam-components/cam-view/cam-view.component';
import { CsvButtonComponent } from './cam-components/csv-button/csv-button.component';
import { BoolIconComponent }  from './cam-components/bool-icon/bool-icon.component';

const appRoutes: Routes = [
    { path: 'ipvd', component: NxIpvdComponent }
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
        ReactiveFormsModule,
        Angular2CsvModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxIpvdComponent,
        CamTableComponent,
        CamViewComponent,
        CsvButtonComponent,
        BoolIconComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxIpvdComponent
    ],
    exports        : [
        NxIpvdComponent
    ]
})

export class IpvdModule { }
