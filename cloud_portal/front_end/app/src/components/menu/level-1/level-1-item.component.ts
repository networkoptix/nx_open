import { Component, Input, OnInit } from '@angular/core';

/* Usage
<nx-menu>
</nx-menu>
*/

@Component({
    selector: 'nx-level-1-item',
    templateUrl: 'level-1-item.component.html',
    styleUrls: []
})
export class NxLevel1ItemComponent implements OnInit {
    @Input() type: string;

    constructor() {
    }

    ngOnInit() {

    }
}
