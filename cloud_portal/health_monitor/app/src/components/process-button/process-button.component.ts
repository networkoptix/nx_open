import { Component, OnInit, Input, ViewEncapsulation } from '@angular/core';

@Component({
    selector: 'nx-process-button',
    templateUrl: 'process-button.component.html',
    styleUrls: ['process-button.component.scss'],
    encapsulation: ViewEncapsulation.None,
})
export class NxProcessButtonComponent implements OnInit {
    @Input() process: any;
    @Input() clickFn: any;
    @Input() buttonText: string;
    @Input() buttonDisabled: boolean;
    @Input() actionType: any;
    @Input() form: any;
    @Input() customClass: any = '';

    buttonClass: string;

    constructor() {
    }

    ngOnInit() {
        if (!this.clickFn) {
            this.clickFn = () => {};
        }

        this.buttonClass = 'btn-primary';
        if (this.actionType) {
            this.buttonClass = 'btn-' + this.actionType;
        }
    }

    touchForm() {
        for (const ctrl in this.form.form.controls) {
            if (this.form.form.controls.hasOwnProperty(ctrl)) {
                this.form.form.get(ctrl).markAsTouched();
                this.form.form.get(ctrl).markAsDirty();
            }
        }
    }

    setFocusToInvalid() {
        for (const ctrl in this.form.form.controls) {
            if (this.form.form.controls.hasOwnProperty(ctrl)) {
                if (this.form.form.get(ctrl).invalid) {
                    // TODO : find how to set element's focus
                    // control.focused = true;
                    return;
                }
            }
        }
    }

    checkForm() {
        if (this.form && !this.form.valid) {
            // Set the form touched
            this.touchForm();
            this.setFocusToInvalid();

            return false;
        } else {
            this.process.run();
        }
    }
}
