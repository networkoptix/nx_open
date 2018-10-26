import { NgModule } from '@angular/core';

import { NxArrowNavDirective }       from './nx-arrow-nav';
import { NxClickElsewhereDirective } from './nx-click-elsewhere';
import { NxFocusMeDirective }        from './nx-focus-me';

@NgModule({
    imports: [],
    declarations: [
        NxArrowNavDirective,
        NxClickElsewhereDirective,
        NxFocusMeDirective
    ],
    entryComponents: [],
    exports: [
        NxArrowNavDirective,
        NxClickElsewhereDirective,
        NxFocusMeDirective
    ]
})
export class DirectivesModule {
}
