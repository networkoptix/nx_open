import { Component, Input, OnInit } from '@angular/core';
import { trigger, style, animate, transition } from '@angular/animations';

import { NxConfigService } from '../../services/nx-config';

const config = new NxConfigService().config;

@Component({
    selector: 'nx-carousel',
    templateUrl: 'carousel.component.html',
    styleUrls: ['carousel.component.scss'],
    animations: [
        trigger('visibilityChange', [
            transition('enter => leave', [
                style({
                    opacity: 1,
                    visibility: 'visible'
                }),
                animate(config.animation.carouselImageLeave, style({ opacity: 0, visibility: 'hidden'}))
            ]),
            transition('* => enter', [
                style({
                    opacity: 0,
                    visibility: 'hidden'
                }),
                animate(config.animation.carouselImageEnter, style({ opacity: 1, visibility: 'visible'}))
            ])
        ])
    ]
})
export class NxCarouselComponent implements OnInit {
    @Input() screenshots: any;

    images: any = [];
    private currentIndex = 0;
    private imageCount: number;

    caption: string;

    constructor() {}

    ngOnInit() {
        this.caption = '';
        this.imageCount = this.screenshots.length;

        if (this.imageCount) {
            this.setCaption();
        }
    }

    private mod(n, m) {
        return ((n % m) + m) % m;
    }

    previousElement(): void {
        this.currentIndex = this.mod((this.currentIndex - 1), this.screenshots.length);
        this.setCaption();
    }

    nextElement(): void {
        this.currentIndex = this.mod((this.currentIndex + 1), this.screenshots.length);
        this.setCaption();
    }

    setIndex(i): void {
        this.currentIndex = i;
        this.setCaption();
    }

    setCaption(): void {
        this.caption = this.screenshots[this.currentIndex].caption;
    }
}
