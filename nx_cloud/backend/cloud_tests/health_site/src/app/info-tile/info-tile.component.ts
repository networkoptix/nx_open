import { Input, Component, OnInit } from '@angular/core';

@Component({
    selector: 'app-info-tile',
    templateUrl: './info-tile.component.html',
    styleUrls: ['./info-tile.component.scss']
})
export class InfoTileComponent implements OnInit {
    @Input() tiles;
    @Input() items;
    @Input() serverTime;

    constructor() {
    }

    ngOnInit() {
    }

    private matchItems(a, b) {
        return (a.key === b.name || b.dimensions && b.dimensions.host === a.key);
    }

    getClassFor(item) {
        if (item.dimension) {
            return (item.value) ? {value: 'nx-error', alert: ''} : {value: 'nx-healthy', alert: ''};
        }

        if (item.value) {
            return {value: 'nx-error', alert: ''};
        }

        // Get aggregates (if any)
        if (item.aggregate) {
            let status = this.items.filter(metric => {
                return item.aggregate.find(aggr =>
                    this.matchItems(aggr, metric)
                );
            }).reduce((total, elm) => {
                let alert = '';
                if (elm.value === 0) {
                    alert = item.aggregate.find(aggr =>
                        this.matchItems(aggr, elm)
                    ).message;

                    if (total.alert !== '') {
                        alert = '<br>' + alert;
                    }
                }
                return {value: total.value |= elm.value, alert: total.alert + alert};
            }, {value: 0, alert: ''});

            status.value = (status.value) ? 'nx-alert' : 'nx-healthy';

            return status;
        }

        return {value: 'nx-healthy', alert: ''};
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
                section.subsections.forEach((subsection, idx) => {
                    let subitem = this.items.filter(item => {
                        if (item.name === subsection.dimension && item.dimensions && item.dimensions.host === subsection.key) {
                            return item;
                        }
                    })[0];

                    section.subsections[idx] = {...subsection, ...subitem}; // extend subsection info
                });
            }

            tile_items.push({...section, ...item}); // extend section info

        });

        return tile_items;
    }
}
