import {
    Component, ElementRef, Input, OnChanges,
    OnInit, SimpleChanges, ViewEncapsulation
} from '@angular/core';
import { NxConfigService } from '../../services/nx-config';
import { NONE_TYPE }       from '@angular/compiler/src/output/output_ast';

/* Usage
<nx-menu>
</nx-menu>
*/

@Component({
    selector: 'nx-menu',
    templateUrl: 'menu.component.html',
    styleUrls: ['menu.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class NxMenuComponent implements OnInit, OnChanges {
    @Input() content: any;

    systemId: any;
    selectedLevel1: string;
    selectedLevel2: string;
    selectedLevel3: string;

    CONFIG: any;


    constructor(private configService: NxConfigService) {
    }

    ngOnInit() {
        this.CONFIG = this.configService.getConfig();
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.content.currentValue) {
            this.selectedLevel1 = changes.content.currentValue.selectedSection;
            this.selectedLevel2 = changes.content.currentValue.selectedSubSection;
            this.selectedLevel3 = changes.content.currentValue.selectedDetailsSection;
        }

        if (changes.content.currentValue.selectedSection) {
            this.systemId = changes.content.currentValue.systemId;
        }
    }

    subLevelItemsFor(item) {
        let levelItems = [];

        // To avoid complicated code this cover only level2 for now ...
        // as only level2 have complex structure
        if (item.level2) {
            levelItems = item.level2.filter((subSection) => {
                if (!this.CONFIG || subSection.id !== this.CONFIG.systemMenu.buttons.id) {
                    return true;
                }
            });
        }

        return levelItems;
    }

    subLevelButtonsFor(item) {
        let buttons: any = [];

        // To avoid complicated code this cover only level2 for now ...
        // as only level2 have complex structure
        if (item.level2) {
            buttons = item.level2.filter((subSection) => {
                if (this.CONFIG && subSection.id === this.CONFIG.systemMenu.buttons.id) {
                    return true;
                }
            })[0] || [] ;
        }

        if (buttons.items && buttons.items.length) {
            buttons = buttons.items;
        }

        return buttons;
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    // *** Breadcrumb for usage of named (auxiliary) router outlet
    // usage: [routerLink]="getItemLink(item)"
    // getItemLink(item){
    //     return [{outlets: { [item.target || 'primary'] : [item.path]}}];
    // }
}
