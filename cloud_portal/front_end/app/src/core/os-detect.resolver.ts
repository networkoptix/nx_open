import { Injectable }            from '@angular/core';
import { Resolve }               from '@angular/router';
import { Observable }            from 'rxjs/Observable'
import { DeviceDetectorService } from "ngx-device-detector";

@Injectable()
export class OsResolver implements Resolve<Observable<string>> {

    deviceInfo: any;

    constructor(private deviceService: DeviceDetectorService) {
        this.deviceInfo = this.deviceService.getDeviceInfo();
    }

    resolve() {
        return this.deviceInfo;
    }
}
