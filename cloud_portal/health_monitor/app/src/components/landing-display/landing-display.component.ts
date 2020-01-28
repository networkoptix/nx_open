import {
    AfterViewInit, Compiler,
    Component, Injector,
    NgModule, NgModuleRef,
    ViewChild,
    ViewContainerRef
}                           from '@angular/core';
import { NxConfigService }  from '../../services/nx-config';

@Component({
    selector : 'landing-display-component',
    template : `
        <ng-container #dynamicTemplate></ng-container>`,
    styleUrls: ['landing-display.component.scss']
})

export class NxLandingDisplayComponent implements AfterViewInit {
    CONFIG: any = {};

    @ViewChild('dynamicTemplate', { read: ViewContainerRef, static: true }) dynamicTemplate;
    @ViewChild('dynamicImage', { read: ViewContainerRef, static: true }) dynamicImage;

    constructor(private _compiler: Compiler,
                private _injector: Injector,
                private _m: NgModuleRef<any>,
                private configService: NxConfigService) {

        this.CONFIG = configService.getConfig();
    }

    ngAfterViewInit() {
        const myTemplateUrl = '/' + this.CONFIG.viewsDir + 'static/landing.html';

        const tmpCmp = Component({
            moduleId: module.id,
            templateUrl: myTemplateUrl
        })(class {
        });

        const tmpModule = NgModule({
            declarations: [tmpCmp]
        })(class {
        });

        this._compiler.compileModuleAndAllComponentsAsync(tmpModule)
            .then((factories) => {
                const factory = factories.componentFactories[0];
                const compRef = factory.create(this._injector, [], null, this._m);
                compRef.instance.name = 'dynamic';

                if (this.CONFIG.previewPath) {
                    // Image src is already compiled with full path
                    // .. so it needs some massaging
                    const images = compRef.location.nativeElement.querySelectorAll('img');
                    images.forEach((img) => {
                        const position = img.src.indexOf('/static');
                        img.src = [img.src.slice(0, position), '/' + this.CONFIG.previewPath, img.src.slice(position)].join('');
                    });
                }

                this.dynamicTemplate.insert(compRef.hostView);
            });
    }
}
