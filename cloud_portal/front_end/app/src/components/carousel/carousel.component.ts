import { Component, Input, OnInit } from "@angular/core";

@Component({
    selector: 'nx-carousel',
    templateUrl: 'carousel.component.html',
    styleUrls: ['carousel.component.scss'],
})
export class NxCarouselComponent implements OnInit {
    @Input() elements: any;
    keys: any;

    currentIndex = 0;
    constructor(){}

    ngOnInit(){
         this.keys = Object.keys(this.elements).filter((element) => {
            return element.match(/screenshot/i);
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
