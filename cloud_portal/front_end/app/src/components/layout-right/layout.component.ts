import { Component, ElementRef, Input, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';

/* Usage
 <nx-right-layout>
    <nx-block first>
         <header>
            Some data (TOP)
         </header>

         <nx-section>
            ...
         </nx-section>
    </nx-block>

    <nx-block side>
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
    selector   : 'nx-right-layout',
    templateUrl: 'layout.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls  : [ 'layout.component.scss' ],
})
export class NxLayoutComponent implements OnInit {

    constructor() {
    }

    ngOnInit() {

    }
}
