import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalEmbedComponent } from './embed.component';

describe('NxModalEmbedComponent', () => {
  let component: NxModalEmbedComponent;
  let fixture: ComponentFixture<NxModalEmbedComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ NxModalEmbedComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalEmbedComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
