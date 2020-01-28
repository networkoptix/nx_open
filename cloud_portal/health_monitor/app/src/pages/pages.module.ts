import { NgModule } from '@angular/core';
import { Angular2CsvModule } from 'angular2-csv';

import { NxHealthModule }            from './health/health.module';

@NgModule({
    imports        : [
        Angular2CsvModule,
        NxHealthModule,
    ],
    declarations   : [],
    entryComponents: [],
    providers      : [],
    exports        : [
        NxHealthModule,
        Angular2CsvModule,
    ]
})
export class PagesModule {
}

