import {
    Component, forwardRef, Input, OnChanges, OnInit, SimpleChanges, ViewEncapsulation
} from '@angular/core';
import { NxConfigService }        from '../../services/nx-config';
import { NG_VALUE_ACCESSOR }      from '@angular/forms';
import { ActivatedRoute }         from '@angular/router';
import { NxUriService }           from '../../services/uri.service';
import { TranslateService }       from '@ngx-translate/core';
import { NxUtilsService }         from '../../services/utils.service';

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
    encapsulation: ViewEncapsulation.None,
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
                label: this.lang.cameraFilters.highRes,
                select: {id: 'maxResolution'}
            },
            {
                label: this.lang.cameraFilters.aptz,
                tagId: 'isAptzSupported'
            },
            {
                label: this.lang.cameraFilters.ptz,
                tagId: 'isPtzSupported'
            },
            {
                label: this.lang.cameraFilters.audio,
                tagId: 'isAudioSupported',
                multiselect: {id: 'hardwareTypes', value: 'camera'}
            },
            {
                label: this.lang.cameraFilters.H265,
                tagId: 'isH265'
            },
            {
                label: this.lang.cameraFilters.encoder,
                multiselect: { id: 'hardwareTypes', value: 'encoder' }
            },
            {
                label: this.lang.cameraFilters.TwWayAudio,
                tagId: 'isTwAudioSupported'
            },
            {
                label: this.lang.cameraFilters.multiSensor,
                multiselect: { id: 'hardwareTypes', value: 'multiSensorCamera' }
            },
            {
                label: this.lang.cameraFilters.fisheye,
                tagId: 'isFisheye'
            },
            {
                label: this.lang.cameraFilters.IO,
                tagId: 'isIoSupported',
                multiselect: { id: 'hardwareTypes', value: 'other' }
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

    ngOnChanges(changes: SimpleChanges) {
        if (changes.vendors) {

            const byCountDESC = NxUtilsService.byParam((elm) => {
                return elm.count;
            }, NxUtilsService.sortDESC);

            const byNameASC = NxUtilsService.byParam((elm) => {
                return elm.name.toLowerCase();
            }, NxUtilsService.sortASC);

            this.remainingVendors = changes.vendors.currentValue.length - this.CONFIG.ipvd.vendorsShown;
            this.vendors = changes.vendors
                    .currentValue
                    .sort(byCountDESC)
                    .slice(0, this.CONFIG.ipvd.vendorsShown)
                    .sort(byNameASC);
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

        if (filter.select && filter.select.id === 'maxResolution') {
            this.filter.selects.find((select) => {
                if (select.id === 'resolution') {
                    select.selected = select.items[select.items.length - 1];
                    queryParams.resolution = select.selected.name;
                }
            });
        } else {
            if (filter.tagId) {
                queryParams.tags = filter.tagId;
                this.filter.tags.find(tag => {
                    if (tag.id === filter.tagId) {
                        tag.value = true;
                    }
                });
            }

            if (filter.multiselect) {
                this.filter.multiselects.find((select) => {
                    if (select.id === filter.multiselect.id) {
                        select.selected.push(select.items.find(item => item.id === filter.multiselect.value).id);
                        queryParams.hardwareTypes = select.selected;
                    }
                });
            }
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
