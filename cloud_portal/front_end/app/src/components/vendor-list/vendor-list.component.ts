import {
    Component, EventEmitter, forwardRef, Input, OnInit, Output
}                                 from '@angular/core';
import { NxConfigService }        from '../../services/nx-config';
import { NG_VALUE_ACCESSOR }      from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';
import { NxUriService }           from '../../services/uri.service';

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
export class NxVendorListComponent implements OnInit {
    @Input() vendors: any;
    @Input() cameras: any;

    CONFIG: any;


    private filters: any = [];
    private filter: any = {};

    constructor(CONFIG: NxConfigService,
                private uri: NxUriService,
                private _router: Router,
                private _route: ActivatedRoute) {

        this.CONFIG = CONFIG.getConfig();

        this.filters = [
            {
                label: 'PTZ cameras', id: 'isPtzSupported'
            },
            {
                label: 'Advanced PTZ cameras', id: 'isAptzSupported'
            },
            {
                label: 'Fisheye Cameras', id: 'isFisheye'
            },
            {
                label: 'Cameras supporting H.265', id: 'isH265'
            },
            {
                label: 'I / O modules', id: 'isIoSupported'
            },
            {
                label: 'Multi-sensor Cameras', id: 'isMultiSensor'
            },
            {
                label: 'Cameras with 2-way audio', id: 'isTwAudioSupported'
            },
            {
                label: 'Extra high resolution cameras', id: 'maxResolution'
            }
        ];
    }

    ngOnInit() {
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
                }
            });
        } else {
            queryParams.tags = filter.id;
        }

        this.uri.updateURI('/campage', queryParams, true);

        // Propagate component's value attribute (model)
        this.propagateChange({ ...this.filter });

        return false;
    }

    setVendor(value) {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};
        queryParams.search = value;

        this.uri.updateURI('/campage', queryParams, true);

        // Propagate component's value attribute (model)
        this.propagateChange({ ...this.filter });

        return false;
    }


}
