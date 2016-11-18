'use strict';
const shm = require('./build/Release/shm.node');
const nodeCleanup = require('node-cleanup');
const cluster = require('cluster');

const uint32Max = Math.pow(2,32) - 1;
const keyMin = 1;
const keyMax = uint32Max - keyMin;
const perm = Number.parseInt('660', 8);
const sizeMin = 1;
/**
 * Max size of shared memory segment in bytes
 * 2GB (0x7fffffff) for 64bit, 1GB for 32bit
 */
const sizeMax = shm.NODE_BUFFER_MAX_LENGTH;

const cleanup = function () {
	try {
		var cnt = shm.detachAll();
		if (cnt > 0)
			console.info('shm segments destroyed:', cnt);
	} catch(exc) { console.error(exc); }
};
process.on('exit', cleanup);
nodeCleanup(cleanup);

/**
 * Types of shared memory object
 */
const BufferType = {
	'Buffer': shm.SHMBT_BUFFER,
	'Int8Array': shm.SHMBT_INT8,
	'Uint8Array': shm.SHMBT_UINT8,
	'Uint8ClampedArray': shm.SHMBT_UINT8CLAMPED,
	'Int16Array': shm.SHMBT_INT16,
	'Uint16Array': shm.SHMBT_UINT16,
	'Int32Array': shm.SHMBT_INT32,
	'Uint32Array': shm.SHMBT_UINT32,
	'Float32Array': shm.SHMBT_FLOAT32, 
	'Float64Array': shm.SHMBT_FLOAT64,
};
const BufferTypeSizeof = {
	'Buffer': 1,
	'Int8Array': 1,
	'Uint8Array': 1,
	'Uint8ClampedArray': 1,
	'Int16Array': 2,
	'Uint16Array': 2,
	'Int32Array': 4,
	'Uint32Array': 4,
	'Float32Array': 4, 
	'Float64Array': 8,
};

/**
 * Create shared memory segment
 * @param {int} count - numer of elements
 * @param {string} typeKey - see keys of BufferType
 * @return {mixed} shared memory buffer/array object
 *  Class depends on param typeKey: Buffer or descendant of TypedArray
 *  Return object has property 'key' - integer key of created shared memory segment
 */
function create(count, typeKey) {
	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	var size1 = BufferTypeSizeof[typeKey];
	var type = BufferType[typeKey];
	var size = size1 * count;
	var countMin = Math.floor(sizeMin / size1),
		countMax = Math.floor(sizeMax / size1);
	if (!(Number.isSafeInteger(count) && count >= countMin && count <= countMax))
		throw new RangeError('Count should be ' + countMin + ' .. ' + countMax);
	let key, res;
	do {
		key = _keyGen();
		res = shm.get(key, size, shm.IPC_CREAT|shm.IPC_EXCL|perm, 0, type);
	} while(!res);
	res.key = key;
	return res;
}

/**
 * Get shared memory segment
 * @param {int} key - integer key of shared memory segment
 * @param {string} typeKey - see keys of BufferType
 * @return {mixed} shared memory buffer/array object, see create()
 */
function get(key, typeKey) {
	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	var type = BufferType[typeKey];
	if (!(Number.isSafeInteger(key) && key >= keyMin && key <= keyMax))
		throw new RangeError('Shm key should be ' + keyMin + ' .. ' + keyMax);
	let res = shm.get(key, 0, 0, 0, type);
	res.key = key;
	return res;
}

/**
 * Detach shared memory segment
 * If there are no other attaches for this segment, it will be destroyed
 * @param {int} key - integer key of shared memory segment
 * @param {bool} forceDestroy - true to destroy even there are other attaches
 * @return {int} count of left attaches or -1 on error
 */
function detach(key, forceDestroy) {
	if (forceDestroy === undefined)
		forceDestroy = false;
	return shm.detach(key, forceDestroy);
}

/**
 * Detach all created and getted shared memory segments
 * Will be automatically called on process exit/termination
 * @return {int} count of destroyed segments
 */
function detachAll() {
	return shm.detachAll();
}

function _keyGen() {
	return keyMin + Math.floor(Math.random() * keyMax);
}

//Exports
module.exports.create = create;
module.exports.get = get;
module.exports.detach = detach;
module.exports.detachAll = detachAll;
module.exports.getTotalSize = shm.getTotalSize;
module.exports.BufferType = BufferType;
module.exports.BufferTypeSizeof = BufferTypeSizeof;
module.exports.SizeMax = sizeMax;
