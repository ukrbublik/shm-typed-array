
/// <reference types="node" />

type Shm<T> = (T & { key?: number });

type ShmMap = {
    Buffer: Shm<Buffer>;
    Int8Array: Shm<Int8Array>;
    Uint8Array: Shm<Uint8Array>;
    Uint8ClampedArray: Shm<Uint8ClampedArray>;
    Int16Array: Shm<Int16Array>;
    Uint16Array: Shm<Uint16Array>;
    Int32Array: Shm<Int32Array>;
    Uint32Array: Shm<Uint32Array>;
    Float32Array: Shm<Float32Array>;
    Float64Array: Shm<Float64Array>;
}

/**
* Create shared memory segment/object.
* Returns null if shm already exists.
*/
export function create<K extends keyof ShmMap = 'Buffer'>(count: number, typeKey?: K, key?: number | string, perm?: string): ShmMap[K] | null;

/**
 * Get shared memory segment/object.
 * Returns null if shm not exists.
 */
export function get<K extends keyof ShmMap = 'Buffer'>(key: number | string, typeKey?: K): ShmMap[K] | null;

/**
 * Detach shared memory segment/object.
 * For System V: If there are no other attaches for this segment, it will be destroyed.
 * Returns 0 on destroy, 1 on detach, -1 on error
 */
export function detach(key: number | string, forceDestoy?: boolean): number;

/**
 * Destroy shared memory segment/object.
 */
export function destroy(key: number | string): boolean;

/**
 * Detach all created and getted shared memory segments/objects.
 * Will be automatically called on process exit/termination.
 */
export function detachAll(): number;

/**
 * Get total size of all *used* shared memory in bytes.
 */
export function getTotalSize(): number;

/**
 * Get total size of all *created* shared memory in bytes.
 */
export function getTotalCreatedSize(): number;

/**
 * Max length of shared memory segment (count of elements, not bytes).
 * 2^31 for 64bit, 2^30 for 32bit.
 */
export const LengthMax: number;

/**
 * Types of shared memory object
 */
export const BufferType: {
    [key: string]: number;
}
