import { Component, Input, OnInit, SimpleChange, SimpleChanges, ViewEncapsulation } from '@angular/core';

/* Usage
 <nx-right-layout>
    <nx-block first-element>
         <header>
            Some data (TOP)
         </header>

         <nx-section>
            ...
         </nx-section>
    </nx-block>

    <nx-block side-element>
         <header>
            Menu (SIDE)
         </header>

         <nx-section>
         ...
         </nx-section>
    </nx-block>

     <nx-block>
         <header>
            ...
         </header>

         <nx-section>
         ...
         </nx-section>
     </nx-block>
</nx-right-layout>
*/

@Component({
    selector   : 'nx-layout-right',
    templateUrl: 'layout.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls  : [ 'layout.component.scss' ],
})
export class NxLayoutRightComponent implements OnInit {

    @Input('loading') loading: any;
    @Input('toggle') toggle: any;
    private _toggle: string;

    constructor() {
    }

    ngOnInit() {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.toggle) {
            this._toggle = changes.toggle.currentValue;
        }
    }
}
