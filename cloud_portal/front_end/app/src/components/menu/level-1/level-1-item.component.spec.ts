import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLevel1ItemComponent } from './level-1-item.component';

describe('NxLevel1ItemComponent', () => {
  let component: NxLevel1ItemComponent;
  let fixture: ComponentFixture<NxLevel1ItemComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxLevel1ItemComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxLevel1ItemComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
