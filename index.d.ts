
/// <reference types="node" />

type Shm<T> = (T & { key: number });

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
* Create shared memory segment.
* Returns null if shm can't be created.
*/
export function create<K extends keyof ShmMap = 'Buffer'>(count: number, typeKey?: K, key?: number): ShmMap[K] | null;

/**
 * Get shared memory segment.
 * Returns null if shm can't be opened.
 */
export function get<K extends keyof ShmMap = 'Buffer'>(key: number, typeKey?: K): ShmMap[K] | null;

/**
 * Detach shared memory segment.
 * If there are no other attaches for this segment, it will be destroyed.
 */
export function detach(key: number, forceDestoy?: boolean): number;

/**
 * Detach all created and getted shared memory segments.
 * Will be automatically called on process exit/termination.
 */
export function detachAll(): number;

/**
 * Get total size of all shared segments in bytes.
 */
export function getTotalSize(): number;

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
