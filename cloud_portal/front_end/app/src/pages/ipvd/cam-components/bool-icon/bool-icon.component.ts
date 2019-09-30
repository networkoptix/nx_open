import { Component, Input, OnInit, SimpleChanges, ViewEncapsulation } from '@angular/core';

@Component({
    selector   : 'nx-bool-icon',
    templateUrl: './bool-icon.component.html',
    styleUrls  : ['./bool-icon.component.scss'],
    encapsulation: ViewEncapsulation.None,
})
export class BoolIconComponent implements OnInit {
    @Input() param: string;
    @Input() value: string;
    @Input() align: string;

    public additional: string;

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

            // maxResolution and sndResolution (secondary stream) are not boolean - we need to overwrite '0x0'
            if ((this.value === '0x0' || this.value === '0') &&
                changes.param &&
                    (changes.param.currentValue === 'maxResolution' ||
                     changes.param.currentValue === 'sndResolution')) {
                this.value = null;
                this.align = 'left';
            }

            // maxFps is not boolean - we need to overwrite '0'
            if (!this.value &&
                changes.param &&
                changes.param.currentValue === 'maxFps') {

                this.value = null;
            }
        }
    }
}
