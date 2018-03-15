import { Component, OnInit } from '@angular/core';
import { NgbModal, ModalDismissReasons } from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'nx-modal',
    templateUrl: './modal/modal.component.html',
    styleUrls: ['./modal/modal.component.scss']
})

export class NxModalComponent implements OnInit {
    closeResult: string;

    constructor(private modalService: NgbModal) {
    }

    open(content) {
        this.modalService.open(content).result.then((result) => {
            this.closeResult = `Closed with: ${result}`;
        }, (reason) => {
            this.closeResult = `Dismissed ${this.getDismissReason(reason)}`;
        });
    }

    private getDismissReason(reason: any): string {
        if (reason === ModalDismissReasons.ESC) {
            return 'by pressing ESC';
        } else if (reason === ModalDismissReasons.BACKDROP_CLICK) {
            return 'by clicking on a backdrop';
        } else {
            return `with: ${reason}`;
        }
    }

    ngOnInit() {
    }

}
