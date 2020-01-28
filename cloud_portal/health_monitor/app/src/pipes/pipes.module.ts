import { NgModule } from '@angular/core';

import { NxUrlSafePipe }       from './nx-url-safe';
import { NxHealthDatePipe } from './health-date';

@NgModule({
    imports        : [],
    declarations   : [
        NxHealthDatePipe,
        NxUrlSafePipe,
    ],
    entryComponents: [],
    exports        : [
        NxHealthDatePipe,
        NxUrlSafePipe
    ],
    providers: [NxHealthDatePipe]
})
export class PipesModule {
}
