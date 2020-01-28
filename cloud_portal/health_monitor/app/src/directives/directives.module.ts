import { NgModule } from '@angular/core';

import { NxArrowNavDirective }        from './nx-arrow-nav';
import { NxClickElsewhereDirective }  from './nx-click-elsewhere';
import { NxFocusMeDirective }         from './nx-focus-me';
import { HighlightPipe }              from './nx-highlight-text';
import { NxScrollHelperDirective }    from './nx-scroll-helper';
import { NxScrollMechanicsDirective } from './nx-scroll-mechanics';

@NgModule({
    imports: [],
    declarations: [
        NxArrowNavDirective,
        NxClickElsewhereDirective,
        NxFocusMeDirective,
        HighlightPipe,
        NxScrollHelperDirective,
        NxScrollMechanicsDirective,
    ],
    entryComponents: [],
    exports: [
        NxArrowNavDirective,
        NxClickElsewhereDirective,
        NxFocusMeDirective,
        HighlightPipe,
        NxScrollHelperDirective,
        NxScrollMechanicsDirective,
    ]
})
export class DirectivesModule {
}
