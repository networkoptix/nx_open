import { ComponentFactoryResolver, Injectable, ViewContainerRef } from '@angular/core';
import { BehaviorSubject, merge } from 'rxjs';
import { distinctUntilChanged, filter, skip } from 'rxjs/operators';
import { NxDialogsService } from '../dialogs/dialogs.service';
import { NxApplyComponent } from '../components/apply/apply.component';
import { NgForm } from '@angular/forms';

/**
 * Allows making subscriptions to variables similar to $watch from AngularJS.
 * Usage:
 * >
 * const someVar = new Watcher<any>;
 * someVar.subscribe((data) => {
 * >>
 * doSomething();
 * >
 * });
 * someVar.value = {data: 'test'};
 * console.log(someVar.value);
 * @class
 */
export class Watcher<T> {
    originalValue: T;
    valueSubject = new BehaviorSubject<T>(undefined);

    get value() {
        return this.valueSubject.getValue();
    }
    set value(data) {
        if (this.value === undefined) {
            this.originalValue = data;
        }
        if (this.value !== data) {
            this.valueSubject.next(data);
        }
    }

    // Resets the value of the watcher to the first value that was not undefined.
    reset() {
        this.valueSubject.next(this.originalValue);
    }
}

@Injectable({
    providedIn: 'root'
})
/**
 * Make sure the route has the ApplyGuard in the canDeactivate part. Other wise navigation will
 * not be blocked when changes are made to the page.
 * How to use the service:
 * >
 * const component = viewContainerRef;
 * const saveProcess = this.processService.createProcess(() => {
 * >>
 *   doSomething();
 * >
 * }, { config });
 * const discardFunction = () => {
 * >>
 *     this.applyService.reset();
 * >
 * };
 * const dataWatcher = new Watcher<string>();
 * this.applyService.initPageWatcher(component, saveProcess, discardFunction, [dataWatcher]);
 * dataWatcher.value = 'first string'; // Nothing happens.
 * dataWatcher.value = 'new string'; // NxApplyComponent becomes visible.
 * @class
 */
export class NxApplyService {
    applyComponentRef: any;
    applyFunction: any;
    component: ViewContainerRef;
    discardFunction: () => void;
    private lockedSubject = new BehaviorSubject<boolean>(undefined);
    private lockedSubscription: any;
    popupActive = false;
    private watchers: any;
    private watchersSubscription: any;
    form: NgForm;

    constructor(private factoryResolver: ComponentFactoryResolver,
                private dialogsService: NxDialogsService) {}

    get locked() {
        return this.lockedSubject.getValue();
    }
    set locked(value) {
        this.lockedSubject.next(value);
    }

    /**
     * Resets all watchers to undefined. This should be used when the component stays the
     * same but the data changes. For an example of how to use this function look at
     * NxSystemUsersComponent.
     */
    hardReset() {
        if (this.watchers) {
            this.watchers.forEach((watcher) => {
                watcher.value = undefined;
            });
        }
        this.locked = false;
        this.setWarn('');
    }

    // Resets all watchers to their first value that wasn't undefined.
    reset() {
        if (this.watchers) {
            this.watchers.forEach((watcher) => {
                watcher.reset();
            });
        }
        this.locked = false;
        this.setWarn('');
    }

    touched() {
        this.locked = true;
    }

    /**
     * Creates the NxApplyComponent for the current page and sets the watchers.
     * @param component The target component where the NxApplyComponent is to be created.
     * @param saveFunction The process that is suppose to run when save is pressed.
     * @param discardFunction The process that is suppose to run when discard is pressed.
     * @param watchers An array of watchers which will trigger the NxApplyComponent to show
     *     if a value on the page has been changed.
     * @param {NgForm=} form Optional form to pass to the process-button
     */
    initPageWatcher(component: ViewContainerRef,
                    saveFunction: any,
                    discardFunction: () => void,
                    watchers: Watcher<any>[],
                    form?: NgForm
    ) {
        this.clearSubscriptions();
        this.component = component;

        this.createComponent();
        this.setSaveFunction(saveFunction);
        this.setDiscardFunction(discardFunction);
        if (form) {
            this.setForm(form);
        }
        this.addWatchers(watchers);
        this.lockedSubscription = this.lockedSubject.subscribe((value) => {
            (<NxApplyComponent>this.applyComponentRef.instance).show = value;
        });
    }

    // The ApplyGuard will call show dialog. For an example look at the settings.module.ts.
    showDialog() {
        // If the apply dialog is active block all other attempts to open it.
        if (this.popupActive) {
            return Promise.resolve(false);
        }
        this.popupActive = true;
        return this.dialogsService.apply(this.applyFunction, this.discardFunction, this.form).then((status) => {
            if (status !== 'applied' && status !== 'discarded') {
                return false;
            }
            this.reset();
            return true;
        }, () => {
            return false;
        }).finally(() => {
            this.popupActive = false;
        });
    }

    canMove() {
        return new Promise<boolean>((resolve) => {
            if (this.locked) {
                this.showDialog().then((state) => {
                    resolve(state);
                });
            } else {
                resolve(!this.locked);
            }
        });
    }

    private clearSubscriptions() {
        if (this.lockedSubscription) {
            this.lockedSubscription.unsubscribe();
        }
        if (this.watchersSubscription) {
            this.watchersSubscription.unsubscribe();
        }
    }

    private createComponent() {
        const compFactory = this.factoryResolver.resolveComponentFactory(NxApplyComponent);
        this.component.clear();
        this.applyComponentRef = this.component.createComponent(compFactory);
        (<NxApplyComponent>this.applyComponentRef.instance).applyVisible = false;
    }

    private setDiscardFunction(func: any) {
        this.discardFunction = func;
        (<NxApplyComponent>this.applyComponentRef.instance).discard = func;
    }

    private setSaveFunction(func: any) {
        this.applyFunction = func;
        (<NxApplyComponent>this.applyComponentRef.instance).save = func;
    }

    public setForm(form: NgForm) {
        this.form = form;
        (<NxApplyComponent>this.applyComponentRef.instance).form = form;
    }

    public setVisible(state?) {
        state = (state === undefined) ? true : state;
        (<NxApplyComponent>this.applyComponentRef.instance).applyVisible = state;
    }

    public setWarn(message) {
        (<NxApplyComponent>this.applyComponentRef.instance).warn = message;
    }

    /**
     * Whats happening here
     * 1) For each watcher create a new observable that only fires when the observable
     *     has a distinct change and the new value is not undefined.
     * 2) Clone and join all of the new observables into one observable.
     * 3) Skip watchers.length to avoid firing when the initial values are set for the watchers.
     * 4) If any of the watchers are changed show the NxApplyComponent and block navigation
     *     until the user saves or discards the changes.
     * @param watchers
     */
    private addWatchers(watchers: Watcher<any>[]) {
        this.watchers = watchers;
        this.watchersSubscription = merge(...watchers.map(watcher => {
            return watcher.valueSubject.pipe(
                distinctUntilChanged(),
                filter((watcher) => watcher !== undefined));
        })).pipe(
            skip(this.watchers.length)
        ).subscribe((res) => {
            this.touched();
        });
    }
}
