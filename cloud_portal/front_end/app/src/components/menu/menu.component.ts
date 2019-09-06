import { Component, Input, OnChanges, OnInit, SimpleChanges } from '@angular/core';
import { ActivatedRoute }                                     from '@angular/router';
import { NxUriService }                                       from '../../services/uri.service';

/* Usage
<nx-menu>
</nx-menu>
*/

@Component({
    selector: 'nx-menu',
    templateUrl: 'menu.component.html',
    styleUrls: ['menu.component.scss']
})
export class NxMenuComponent implements OnInit, OnChanges {
    @Input() content: any;

    selected: string;

    constructor() {
    }

    ngOnInit() {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.content.currentValue.selectedSection) {
            this.selected = changes.content.currentValue.selectedSection;

        }
    }

    // *** Breadcrumb for usage of named (auxiliary) router outlet
    // usage: [routerLink]="getItemLink(item)"
    // getItemLink(item){
    //     return [{outlets: { [item.target || 'primary'] : [item.path]}}];
    // }
}
