import { Component, Input, OnInit } from '@angular/core';

/* Usage
*/

@Component({
    selector: 'nx-level-3-item',
    templateUrl: 'level-3-item.component.html',
    styleUrls: ['level-3-item.component.scss']
})
export class NxLevel3ItemComponent implements OnInit {
    @Input() item: {};

    constructor() {
    }

    ngOnInit() {

    }
}
