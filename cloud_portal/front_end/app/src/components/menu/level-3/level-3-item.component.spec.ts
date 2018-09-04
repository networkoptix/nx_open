import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLevel3ItemComponent } from './level-3-item.component';

describe('NxLevel3ItemComponent', () => {
  let component: NxLevel3ItemComponent;
  let fixture: ComponentFixture<NxLevel3ItemComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxLevel3ItemComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxLevel3ItemComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
