import { Component, OnInit, Input, Output, EventEmitter } from '@angular/core';
import { FormControl } from '@angular/forms';

@Component({
    selector: 'nx-process-button',
    templateUrl: './components/process-button/process-button.component.html',
    styleUrls: []
})
export class NxProcessButtonComponent implements OnInit {
    // @Output() process = new EventEmitter();

    @Input() process: any;

    @Input() buttonText: string;
    @Input() buttonDisabled: boolean;
    @Input() actionType: boolean;
    @Input() form: any;

    buttonClass: string;
    processing: false;

    constructor() {
    }

    ngOnInit() {
        this.buttonClass = 'btn-primary';
        if (this.actionType) {
            this.buttonClass = 'btn-' + this.actionType;
        }
    }

    touchForm() {
        for (const ctrl in this.form.form.controls) {
            this.form.form.get(ctrl).markAsTouched();
        }
    }

    setFocusToInvalid() {
        console.log('ctrls:', this.form.form.controls);

        for(const ctrl in this.form.form.controls) {
            const control = this.form.form.get(ctrl);
            // console.log('CTRL:', control);
            if (control.invalid) {
                // TODO : find how to set element's focus
                // control.focused = true;
                return;
            }
        }
    }

    checkForm() {
        if (this.form && !this.form.valid) {
            //Set the form touched
            this.touchForm();
            this.setFocusToInvalid();

            return false;
        } else {
            this.process.run();
        }
    }

}
