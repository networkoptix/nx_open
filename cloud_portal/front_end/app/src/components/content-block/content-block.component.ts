import {
    Component, Input, OnInit
} from '@angular/core';

/* Usage
<nx-content-block
      title=[TITLE]
      content=[CONTENT]
      type=['gray' | empty]>
</nx-content-block>
*/

@Component({
    selector: 'nx-content-block',
    templateUrl: 'content-block.component.html',
    styleUrls: ['content-block.component.scss'],
})
export class NxContentBlockComponent implements OnInit {
    @Input() title: string;
    @Input() content: any;
    @Input() type: string;


    ngOnInit() {
        if (!this.content) {
            this.content = {
                sections: []
            };
        }

    }

}
