import { Component, Input, OnChanges, OnInit, SimpleChanges } from '@angular/core';
import { NxConfigService }                                    from '../../../services/nx-config';
import { NxMenuService }                                      from '../menu.service';

/* Usage
*/

@Component({
    selector: 'nx-level-1-item',
    templateUrl: 'level-1-item.component.html',
    styleUrls: ['level-1-item.component.scss']
})
export class NxLevel1ItemComponent implements OnInit, OnChanges {
    @Input() base: any = {};
    @Input() item: any = {};
    @Input() selected: boolean;

    itemPath: string;

    CONFIG: any;

    constructor(private configService: NxConfigService,
                private menuService: NxMenuService,
    ) {
        this.CONFIG = this.configService.getConfig();
    }

    ngOnInit() {
        this.itemPath = this.base;
        this.itemPath += (this.item.path !== '') ? '/' + this.item.path : '';
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.base && changes.base.currentValue) {
            this.itemPath = changes.base.currentValue;
            this.itemPath += (this.item.path !== '') ? '/' + this.item.path : '';
        }
    }

    menuClick(sectionId) {
        this.menuService.setSection(sectionId);
    }
}
