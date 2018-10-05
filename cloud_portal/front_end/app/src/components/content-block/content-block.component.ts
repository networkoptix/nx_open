import { Component, Input, OnInit } from '@angular/core';

/* Usage
<nx-content-block type?=['gray' | empty]>
    <div card-header>
        TITLE
    </div>
    <div card-body>
        BODY
    </div>

    <!-- ngFor -->
    <div section-title>
        Section title
    </div>
    <div section-content>
        SECTION
    </div>
    <!-- ngFor -->
</nx-content-block>
*/

@Component({
    selector   : 'nx-content-block',
    templateUrl: 'content-block.component.html',
    styleUrls  : [ 'content-block.component.scss' ],
})
export class NxContentBlockComponent implements OnInit {
    @Input('type') class: string;

    constructor() {
    }

    ngOnInit() {
    }
}
