import { Input, Component, OnInit } from '@angular/core';

@Component({
    selector: 'app-info-tile',
    templateUrl: './info-tile.component.html',
    styleUrls: ['./info-tile.component.scss']
})
export class InfoTileComponent implements OnInit {
    @Input() tiles;
    @Input() items;

    visible: boolean;

    constructor() {
    }

    ngOnInit() {
        this.visible = true;
    }

    getClassFor(item) {

        if (item.dimension){
            return (item.value === 0) ? 'nx-error' : 'nx-healthy';
        }

        if (!item.value){
            return 'nx-error';
        }

        if (item.aggregate) {
            let status = this.items.filter(metric => {
                return item.aggregate.find(aggr =>
                   aggr.key === metric.name || metric.dimensions && metric.dimensions.host === aggr.key
                );
            }).reduce((a, b) => {
                return {value: a.value &= b.value};
            }, { value : 1 });

            console.log('item -> ', item);
            return (status.value === 0) ? 'nx-alert' : 'nx-healthy' ;
        }

        return 'nx-healthy';
    }

    getTileItemsFor(tile) {
        if (!this.items) {
            return;
        }

        let tile_items = [];

        // Use structural json and update it with data from the payload

        tile.sections.forEach((section) => {
            let item = this.items.filter(item => {
                if (item.name === section.key) {
                    return item;
                }
            })[0];

            if (section.subsections) {
                let idx = 0;
                section.subsections.forEach((subsection) => {
                    let subitem = this.items.filter(item => {

                        if (item.name === subsection.dimension && item.dimensions && item.dimensions.host === subsection.key) {
                            return item;
                        }
                    })[0];

                    section.subsections[idx] = {...subsection, ...subitem}; // extend subsection info
                    idx++;
                });
            }

            tile_items.push({...section, ...item}); // extend section info

        });

        return tile_items;
    }
}
