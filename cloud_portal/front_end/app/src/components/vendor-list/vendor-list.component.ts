import {
    Component, EventEmitter, forwardRef, Input, OnChanges, OnInit, Output, SimpleChanges
}                                 from '@angular/core';
import { NxConfigService }        from '../../services/nx-config';
import { NG_VALUE_ACCESSOR }      from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';
import { NxUriService }           from '../../services/uri.service';
import { TranslateService }       from '@ngx-translate/core';

/* USAGE
 <nx-vendor-list
 vendors=[]
 cameras=[]
 [(ngModel)]="filterModel">
 </nx-vendor-list>
 */

@Component({
    selector   : 'nx-vendor-list',
    templateUrl: 'vendor-list.component.html',
    styleUrls  : ['vendor-list.component.scss'],
    providers  : [{
        provide    : NG_VALUE_ACCESSOR,
        useExisting: forwardRef(() => NxVendorListComponent),
        multi      : true
    }]
})
export class NxVendorListComponent implements OnInit, OnChanges {
    @Input() vendors: any;
    @Input() cameras: any;

    CONFIG: any;

    public debug: boolean;
    public filters: any = [];
    public remainingVendors: number;

    private lang: any = {};
    private uriPath: string;
    private filter: any = {};
    private ASC = true;
    private DESC = false;

    constructor(CONFIG: NxConfigService,
                private translate: TranslateService,
                private uri: NxUriService,
                private _route: ActivatedRoute) {

        this.CONFIG = CONFIG.getConfig();
        this.debug = false;
        this.uriPath = '/' + this._route.snapshot.url[0].path;
        this.lang = this.translate.translations[this.translate.currentLang];

        this.filters = [
            {
                label: this.lang.cameraFilters.ptz, id: 'isPtzSupported'
            },
            {
                label: this.lang.cameraFilters.aptz, id: 'isAptzSupported'
            },
            {
                label: this.lang.cameraFilters.fisheye, id: 'isFisheye'
            },
            {
                label: this.lang.cameraFilters.H265, id: 'isH265'
            },
            {
                label: this.lang.cameraFilters.IO, id: 'isIoSupported'
            },
            {
                label: this.lang.cameraFilters.multiSensor, id: 'isMultiSensor'
            },
            {
                label: this.lang.cameraFilters.TwWayAudio, id: 'isTwAudioSupported'
            },
            {
                label: this.lang.cameraFilters.highRes, id: 'maxResolution'
            }
        ];
    }

    ngOnInit() {
        this.uri
            .getURI()
            .subscribe(params => {
                if (params.debug !== undefined) {
                    this.debug = true;
                }
            });
    }

    // Form control functions
    // The method set in registerOnChange to emit changes back to the form
    private propagateChange = (_: any) => {
    };

    writeValue(value: any) {
        if (value) {
            this.filter = value;
        }
    }

    byParam (a, b, param, order) {
        if (a[param] < b[param]) {
            return (order) ? -1 : 1;
        }
        if (a[param] > b[param]) {
            return (order) ? 1 : -1;
        }
        return 0;
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.vendors) {
            this.remainingVendors = changes.vendors.currentValue.length - this.CONFIG.ipvd.vendorsShown;
            this.vendors = changes.vendors
                    .currentValue
                    .sort((a, b) => this.byParam(a, b, 'count', this.DESC))
                    .slice(0, this.CONFIG.ipvd.vendorsShown)
                    .sort((a, b) => this.byParam(a, b, 'name', this.ASC));
        }
    }

    /**
     * Set the function to be called
     * when the control receives a change event.
     */
    registerOnChange(fn) {
        this.propagateChange = fn;
    }

    /**
     * Set the function to be called
     * when the control receives a touch event.
     */
    registerOnTouched(fn: () => void): void {
    }

    setFilter(filter) {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};

        if (filter.id === 'maxResolution') {
            this.filter.selects.find((select) => {
                if (select.id === 'resolution') {
                    queryParams.resolution = select.items[select.items.length - 1].name;
                    select.selected = select.items[select.items.length - 1];
                }
            });
        } else {
            queryParams.tags = filter.id;
            this.filter.tags.find(tag => {
                if (tag.id === filter.id) {
                    tag.value = true;
                }
            });
        }

        this.uri.updateURI('/ipvd', queryParams, true);

        // Propagate component's value attribute (model)
        this.propagateChange({ ...this.filter });

        return false;
    }

    setVendor(vendor) {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};

        this.filter.multiselects.find((select) => {
            if (select.id === 'vendors') {
                select.selected.push(vendor.name);

                queryParams[select.id] = select.selected;

                this.uri.updateURI(this.uriPath, queryParams, true);
            }
        });

        // Propagate component's value attribute (model)
        this.propagateChange({ ...this.filter });

        return false;
    }
}
