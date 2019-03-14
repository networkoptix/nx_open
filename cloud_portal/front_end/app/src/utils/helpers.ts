
export interface Array {
    move(arr: Array, old_index: number, new_index: number): void;
}

export class Utils {

    constructor(private array: Array) {
    }

    static move (arr, old_index, new_index): Array {
        while (old_index < 0) {
            old_index += arr.length;
        }
        while (new_index < 0) {
            new_index += arr.length;
        }
        if (new_index >= arr.length) {
            let k = new_index - arr.length;
            while ((k--) + 1) {
                arr.push(undefined);
            }
        }
        arr.splice(new_index, 0, arr.splice(old_index, 1)[0]);
        return arr;
    };
}
