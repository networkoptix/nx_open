import { Component, Input, OnInit } from '@angular/core';

/* Usage
*/

@Component({
    selector: 'nx-level-2-item',
    templateUrl: 'level-2-item.component.html',
    styleUrls: ['level-2-item.component.scss']
})
export class NxLevel2ItemComponent implements OnInit {
    @Input() item: {};

    constructor() {
    }

    ngOnInit() {

    }
}
