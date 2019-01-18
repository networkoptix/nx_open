import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule, ReactiveFormsModule }          from '@angular/forms';
import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { TranslateModule }      from '@ngx-translate/core';
import { Angular2CsvModule }    from 'angular2-csv';

import { NxCampageComponent } from './campage.component';
import { ComponentsModule }   from '../../components/components.module';
import { CamTableComponent } from './cam_components/cam-table/cam-table.component';
import { CamViewComponent } from './cam_components/cam-view/cam-view.component';
import { SearchComponent } from './cam_components/search/search.component';
import { CsvButtonComponent } from './cam_components/csv-button/csv-button.component';
import { BoolIconComponent } from './cam_components/bool-icon/bool-icon.component';

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
        ReactiveFormsModule,
        Angular2CsvModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxCampageComponent,
        CamTableComponent,
        CamViewComponent,
        SearchComponent,
        CsvButtonComponent,
        BoolIconComponent,
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
