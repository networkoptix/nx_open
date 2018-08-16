import { TimeCheckedPipe } from './time-checked.pipe';

describe('TimeCheckedPipe', () => {
    it('create an instance', () => {
        const pipe = new TimeCheckedPipe();
        expect(pipe).toBeTruthy();
    });
});
