import { Component, ViewChild } from "@angular/core";
import { NgForm }               from "@angular/forms";

@Component({
    selector   : "sandbox-component",
    templateUrl: "sandbox.component.html",
    styleUrls  : [ "sandbox.component.scss" ]
})

export class NxSandboxComponent {
    click: any;
    blah: string;
    group: string;
    agree: boolean;
    show: boolean;
    show5: boolean;
    edit: boolean;

    submitted = false;

    @ViewChild("testForm") public testForm: NgForm;

    private setupDefaults() {

        this.show = false;
        this.show5 = false;
        this.blah = "blah1";
        this.group = "Tsanko";
        this.agree = false;
        this.edit = false;

    }

    constructor() {
        this.setupDefaults();
    }

    private touchForm(form) {
        for (const ctrl in form.form.controls) {
            if (form.form.controls.hasOwnProperty(ctrl)) {
                form.form.get(ctrl).markAsTouched();
            }
        }
    }

    onSubmit() {
        if (this.testForm && !this.testForm.valid) {
            // Set the form touched
            this.touchForm(this.testForm);

            return false;
        }

        this.submitted = true;
        window.alert("SUBMIT!");
    }
}

