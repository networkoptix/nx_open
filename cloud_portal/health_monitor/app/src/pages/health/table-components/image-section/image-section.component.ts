import { AfterViewInit, Component, ElementRef, Input, OnChanges, OnDestroy, ViewChild } from '@angular/core';
import { NxHealthService } from '../../health.service';
import { fromEvent, Subscription } from 'rxjs';
import { debounceTime } from 'rxjs/operators';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

interface ThumbNail {
    loaded: boolean;
    time: string;
    url: string;
}

@AutoUnsubscribe()
@Component({
    selector   : 'nx-image-section',
    templateUrl: './image-section.component.html',
    styleUrls: ['./image-section.component.scss']
})
export class NxImageSectionComponent implements OnChanges, AfterViewInit, OnDestroy {
    @Input() cameraInfo: any;
    @ViewChild('imageWidth', {static: false}) imageSize: ElementRef;
    cameraId: string;
    changeRow: boolean;
    ready: boolean;
    state: string;
    thumbnails: ThumbNail[];

    fromEventSubscription: Subscription;

    constructor(private healthService: NxHealthService) {
        this.thumbnails = [];
        this.ready = false;
    }

    ngOnDestroy() {}

    ngAfterViewInit() {
        if (typeof this.imageSize === 'undefined') {
            return;
        }
        this.changeRow = this.imageSize.nativeElement.offsetWidth < 360;
        this.fromEventSubscription = fromEvent(window, 'resize').pipe(debounceTime(10)).subscribe((e) => {
            this.changeRow = this.imageSize.nativeElement.offsetWidth < 360;
        });
    }

    ngOnChanges(changes: any) {
        const cameraInfo = changes.cameraInfo && changes.cameraInfo.currentValue;
        if (!cameraInfo) {
            return;
        }

        this.ready = false;
        this.cameraId = cameraInfo.id;
        this.state = cameraInfo.availability.status.text;
        this.thumbnails = Object.values(cameraInfo)
            .filter((cameraProd: any) => cameraProd.thumbnail)
            .map((cameraProp: any) => {
                const time = cameraProp.thumbnail.text;
                return {
                    loaded: false,
                    time,
                    url: this.healthService.system.mediaserver.previewUrl(this.cameraId, time)
                };
            }).sort((a: any, b: any) => {
                if (a.time === 'now') {
                    return -1;
                } else if (b.time === 'now') {
                    return 1;
                }
                return a.time < b.time ? -1 : 1;
            });
    }


    showPreloader() {
        setTimeout(() => this.ready = this.thumbnails.every((thumbnail) => thumbnail.loaded));
    }
}
