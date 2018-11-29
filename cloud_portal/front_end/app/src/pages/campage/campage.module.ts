import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule, ReactiveFormsModule }          from '@angular/forms';
import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { TranslateModule }      from '@ngx-translate/core';
import { Angular2CsvModule }    from "angular2-csv";

import { NxCampageComponent } from './campage.component';
import { ComponentsModule }   from '../../components/components.module';
import { CamTableComponent } from './temp_components/cam-table/cam-table.component';
import { CamViewComponent } from './temp_components/cam-view/cam-view.component';
import { SearchComponent } from './temp_components/search/search.component';
import { DropdownsModule } from '../../dropdowns/dropdowns.module';
import { CsvButtonComponent } from './temp_components/csv-button/csv-button.component';

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
        DropdownsModule,
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
