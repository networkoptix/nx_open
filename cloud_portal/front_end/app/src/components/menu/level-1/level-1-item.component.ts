import { Component, Input, OnInit } from '@angular/core';

/* Usage
*/

@Component({
    selector: 'nx-level-1-item',
    templateUrl: 'level-1-item.component.html',
    styleUrls: ['level-1-item.component.scss']
})
export class NxLevel1ItemComponent implements OnInit {
    @Input() item: {};

    constructor() {
    }

    ngOnInit() {

    }
}
