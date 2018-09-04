import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLevel2ItemComponent } from './level-2-item.component';

describe('NxLevel2ItemComponent', () => {
  let component: NxLevel2ItemComponent;
  let fixture: ComponentFixture<NxLevel2ItemComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxLevel2ItemComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxLevel2ItemComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
