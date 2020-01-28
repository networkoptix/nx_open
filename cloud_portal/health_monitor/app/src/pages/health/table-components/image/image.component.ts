import { Component, EventEmitter, Input, OnChanges, Output } from '@angular/core';

@Component({
    selector: 'nx-health-image',
    templateUrl: './image.component.html',
    styleUrls: ['./image.component.scss']
})
export class NxImageComponent implements OnChanges {
    @Input() isPrimary: boolean;
    @Input() state: string;
    @Input() time: string;
    @Input() url: string;
    @Output() loaded = new EventEmitter<boolean>();

    ngOnChanges(changes) {
        if (this.state === undefined) {
            this.state = '';
        }
        if (this.state !== 'Online' && this.state !== 'Recording') {
            this.url = '';
            this.loaded.emit(true);
        }
    }
}
