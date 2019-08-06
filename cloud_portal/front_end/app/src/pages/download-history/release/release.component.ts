import {
    Component,
    OnInit,
    Input,
    Inject
} from '@angular/core';

@Component({
    selector   : 'nx-release',
    templateUrl: 'release.component.html',
    styleUrls  : ['release.component.scss']
})
export class ReleaseComponent implements OnInit {
    @Input() release: any;
    @Input() linkbase: any;

    constructor(@Inject('languageService') private language: any) {
    }

    ngOnInit(): void {
    }
}

