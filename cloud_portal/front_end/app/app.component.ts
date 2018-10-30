import { Location }              from '@angular/common';
import { Component, OnInit }     from '@angular/core';
import { TranslateService }      from '@ngx-translate/core';
import { CookieService }         from 'ngx-cookie-service';
import { DeviceDetectorService } from 'ngx-device-detector';

@Component({
    selector: 'nx-app',
    template: `
        <router-outlet></router-outlet>
        <div ng-view="" ng-model-options="{ updateOn: 'blur' }"></div>
    `
})

export class AppComponent {
    deviceInfo: any;
    allowedDevices: {};
    hlsIsSupported: boolean;

    constructor(private cookieService: CookieService,
                private deviceService: DeviceDetectorService,
                private location: Location,
                translate: TranslateService) {

        // TODO: Componentize this
        this.allowedDevices = {
            windows: {
                ie     : 10,
                safari : 10,
                chrome : 64,
                firefox: 60
            },
            mac    : {
                safari : 10,
                chrome : 64,
                firefox: 60
            },
            linux  : {
                chrome : 64,
                firefox: 60
            }
        };

        this.deviceInfo = this.deviceService.getDeviceInfo();
        let allowedDevice = this.allowedDevices[ this.deviceInfo.os ];

        // Special case for Kyle's robot tests
        // ... device detector doesn't detect it correctly
        if (this.deviceInfo.userAgent.indexOf('HeadlessChrome') > -1) {
            allowedDevice = undefined;
        }

        if (allowedDevice !== undefined) {
            const allowedVersion = allowedDevice[ this.deviceInfo.browser ] || 0;
            const majorVersion = this.deviceInfo.browser_version.split('.')[ 0 ];

            if (majorVersion < allowedVersion) {
                // redirect
                this.location.go('/browser');
            }
        } // else -> unknown platform or device ... cross fingers and hope for the best

        const langCookie = this.cookieService.get('language');
        const lang = langCookie || translate.getBrowserCultureLang().replace('-', '_');

        // this language will be used as a fallback when a translation
        // isn't found in the current language
        translate.setDefaultLang('en_US');

        // the lang to use, if the lang isn't available, it will use
        // the current loader to get them
        translate.use(lang);
    }
}
