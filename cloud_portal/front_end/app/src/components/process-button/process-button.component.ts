import { Component, OnInit, Input } from '@angular/core';

@Component({
    selector: 'nx-process-button',
    templateUrl: 'process-button.component.html',
    styleUrls: []
})
export class NxProcessButtonComponent implements OnInit {
    @Input() process: any;
    @Input() buttonText: string;
    @Input() buttonDisabled: boolean;
    @Input() actionType: any;
    @Input() form: any;

    buttonClass: string;

    constructor() {
    }

    ngOnInit() {
        this.buttonClass = 'btn-primary';
        if (this.actionType) {
            this.buttonClass = 'btn-' + this.actionType;
        }
    }

    touchForm() {
        for (let ctrl in this.form.form.controls) {
            this.form.form.get(ctrl).markAsTouched();
        }
    }

    setFocusToInvalid() {
        for(let ctrl in this.form.form.controls) {
            if (this.form.form.get(ctrl).invalid) {
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
