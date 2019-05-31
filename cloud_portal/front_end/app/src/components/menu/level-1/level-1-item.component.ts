import { Component, Input, OnInit } from '@angular/core';

/* Usage
*/

@Component({
    selector: 'nx-level-1-item',
    templateUrl: 'level-1-item.component.html',
    styleUrls: ['level-1-item.component.scss']
})
export class NxLevel1ItemComponent implements OnInit {
    @Input() base: any = {};
    @Input() item: any = {};
    @Input() selected: boolean;

    itemPath: string;

    constructor() {
    }

    ngOnInit() {
        this.itemPath = this.base;
        this.itemPath += (this.item.path !== '') ? '/' + this.item.path : '';
    }
}
