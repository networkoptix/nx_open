import { Component, Input, OnInit, SimpleChanges } from '@angular/core';

@Component({
    selector   : 'nx-bool-icon',
    templateUrl: './bool-icon.component.html',
    styleUrls  : ['./bool-icon.component.scss']
})
export class BoolIconComponent implements OnInit {
    @Input() param: string;
    @Input() value: string;

    private additional: string;

    constructor() {
    }

    ngOnInit() {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.value) {
            this.value = changes.value.currentValue;
            this.additional = '';

            if (this.value && changes.param && changes.param.currentValue === 'isAptzSupported') {
                this.additional = 'Adv.';
            }

            if (this.value && changes.param && changes.param.currentValue === 'isTwAudioSupported') {
                this.additional = '2-way';
            }
        }
    }
}
