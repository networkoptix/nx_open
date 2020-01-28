import { Component, Input, OnInit, ViewEncapsulation } from '@angular/core';

/* Usage
*/

@Component({
    selector: 'nx-level-3-item',
    templateUrl: 'level-3-item.component.html',
    styleUrls: ['level-3-item.component.scss']
})
export class NxLevel3ItemComponent implements OnInit {
    @Input() base: any = {};
    @Input() item: any = {};
    @Input() selected: boolean;
    @Input() first: boolean;

    itemPath: string;
    isEnabled: boolean;

    constructor() {
    }

    ngOnInit() {
        this.itemPath = this.base;
        this.itemPath += (this.item.path !== '') ? '/' + this.item.path : '';
        this.isEnabled = this.item.isEnabled === undefined ? true : this.item.isEnabled;
    }
}
