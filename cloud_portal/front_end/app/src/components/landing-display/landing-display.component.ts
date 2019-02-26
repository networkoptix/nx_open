import { Location }        from '@angular/common';
import {
    AfterViewInit,
    Compiler,
    Component, Inject,
    Injector, NgModule,
    NgModuleRef,
    OnInit,
    ViewChild,
    ViewContainerRef
}                          from '@angular/core';
import { NxConfigService } from '../../services/nx-config';
import { BrowserModule }   from '@angular/platform-browser';

@Component({
    selector : 'landing-display-component',
    template : `
        <ng-container #dynamicTemplate></ng-container>`,
    styleUrls: ['landing-display.component.scss']
})

export class NxLandingDisplayComponent implements AfterViewInit, OnInit {
    CONFIG: any = {};

    @ViewChild('dynamicTemplate', { read: ViewContainerRef }) dynamicTemplate;
    @ViewChild('dynamicImage', { read: ViewContainerRef }) dynamicImage;

    constructor(private _compiler: Compiler,
                private _injector: Injector,
                private _m: NgModuleRef<any>,
                private config: NxConfigService) {

        this.CONFIG = config.getConfig();
    }

    ngOnInit(): void {
    }

    ngAfterViewInit() {
        this.CONFIG = this.config.getConfig();
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
