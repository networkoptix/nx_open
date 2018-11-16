import { Component, Input, OnInit } from '@angular/core';
import { trigger, style, animate, transition } from '@angular/animations';


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
                animate('0.5s ease-out', style({ opacity: 0, visibility: 'hidden'}))
            ]),
            transition('* => enter', [
                style({
                    opacity: 0,
                    visibility: 'hidden'
                }),
                animate('0.5s ease-in', style({ opacity: 1, visibility: 'visible'}))
            ])
        ])
    ]
})
export class NxCarouselComponent implements OnInit {
    @Input() elements: any;
    keys: any;

    private currentIndex = 0;
    constructor(){}

    ngOnInit(){
         this.keys = Object.keys(this.elements).filter((element) => {
            return element.match(/screenshot/i) && this.elements[element];
         });
         this.keys = this.keys.sort();
    }

    private mod(n, m) {
        return ((n % m) + m) % m;
    }

    previousElement(){
        this.currentIndex = this.mod((this.currentIndex-1), this.keys.length);
    }

    nextElement(){
        this.currentIndex = this.mod((this.currentIndex + 1), this.keys.length);
    }
}
