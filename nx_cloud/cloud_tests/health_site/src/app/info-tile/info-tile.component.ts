import { Input, Component, OnInit, OnChanges } from '@angular/core';
import * as moment                             from 'moment';

@Component({
    selector: 'app-info-tile',
    templateUrl: './info-tile.component.html',
    styleUrls: ['./info-tile.component.scss']
})
export class InfoTileComponent implements OnInit, OnChanges {
    @Input() tiles;
    @Input() items;

    constructor() {
    }

    ngOnInit() {
    }

    ngOnChanges() {
        this.tile_items = this.items || [];
    }

    getClassFor(item) {
        return (item) ? 'healthy' : 'error';
    }

    getTileItemsFor(tile) {
        if (!this.items) {
            return;
        }

        let tile_items = [];

        tile.sections.forEach((section) => {
            let item = this.items.filter(item => {
                if (item.name === section.key) {
                    return item;
                }
            })[0];

            tile_items.push({...section, ...item});

        });

        return tile_items;
    }
}
