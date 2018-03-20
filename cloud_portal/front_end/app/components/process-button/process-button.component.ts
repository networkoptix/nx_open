import { Component, OnInit, Input } from '@angular/core';
import { FormControl } from '@angular/forms';


@Component({
    selector: 'nx-process-button',
    templateUrl: './components/process-button/process-button.component.html',
    styleUrls: []
})
export class NxProcessButtonComponent implements OnInit {
    @Input() buttonText: string;
    @Input() process: Function;
    @Input() buttonDisabled: boolean;
    @Input() actionType: boolean;
    @Input() form: any;

    buttonClass: string;

    constructor() {
    }

    ngOnInit() {
        this.buttonClass = 'btn-primary';
        if (this.actionType) {
            this.buttonClass = 'btn-' + this.actionType;
        }

        this.buttonDisabled = this.buttonDisabled || true;
        // this.process = {
        //     processing : false
        // }
    }

    touchForm() {
        for (const ctrl in this.form.form.controls) {
            this.form.form.get(ctrl).markAsTouched();
        }

        // angular.forEach(form.errors, function (field) {
        //     angular.forEach(field, function (errorField) {
        //         if (typeof(errorField.$touched) != 'undefined') {
        //             errorField.$setTouched();
        //         } else {
        //             this.touchForm(errorField); // Embedded form - go recursive
        //         }
        //     })
        // });
    }

    setFocusToInvalid() {
        console.log('ctrls:', this.form.form.controls);

        for(const ctrl in this.form.form.controls) {
            const control = this.form.form.get(ctrl);
            // console.log('CTRL:', control);
            if (control.invalid) {
                // control.focused = true;
                return;
            }
        }
        // $timeout(function () {
        //     $('[name="' + this.form.$name + '"]').find('.ng-invalid:visible:first').focus();
        // });
    }

    checkForm() {
        if (this.form && !this.form.valid) {
            //Set the form touched
            this.touchForm();
            this.setFocusToInvalid();

            console.log('ctrls:', this.form.form.controls);

            return false;
        } else {
            this.process();
        }
    }

}
