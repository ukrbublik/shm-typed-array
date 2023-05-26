'use strict';
const buildDir = process.env.DEBUG_SHM == 1 ? 'Debug' : 'Release';
const shm = require('./build/' + buildDir + '/shm.node');

const uint32Max = Math.pow(2,32) - 1;
const keyMin = 1;
const keyMax = uint32Max - keyMin;
const lengthMin = 1;
/**
 * Max length of shared memory segment (count of elements, not bytes)
 */
const lengthMax = shm.NODE_BUFFER_MAX_LENGTH;

const cleanup = function () {
	try {
		var cnt = shm.detachAll();
		if (cnt > 0)
			console.info('shm segments destroyed:', cnt);
	} catch(exc) { console.error(exc); }
};
process.on('exit', cleanup);

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
 * Create System V or POSIX shared memory
 * @param {int} count - number of elements
 * @param {string} typeKey - see keys of BufferType
 * @param {int/string/null} key - integer key for System V shared memory segment, or null to autogenerate,
 *  or string name for POSIX shared memory object, should start with '/'.
 * @param {string} permStr - permissions, default is 660
 * @return {mixed/null} shared memory buffer/array object, or null if already exists with provided key
 *  Class depends on param typeKey: Buffer or descendant of TypedArray.
 *  For System V: returned object has property 'key' - integer key of created shared memory segment
 */
function create(count, typeKey /*= 'Buffer'*/, key /*= null*/, permStr /*= '660'*/) {
	if (typeof key === 'string') {
		return createPosix(key, count, typeKey, permStr);
	}

	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (key === undefined)
		key = null;
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	if (key !== null) {
		if (!(Number.isSafeInteger(key) && key >= keyMin && key <= keyMax))
			throw new RangeError('Shm key should be ' + keyMin + ' .. ' + keyMax);
	}
	if (permStr === undefined || isNaN( Number.parseInt(permStr, 8)))
		permStr = '660';
	const perm = Number.parseInt(permStr, 8);

	var type = BufferType[typeKey];
	//var size1 = BufferTypeSizeof[typeKey];
	//var size = size1 * count;
	if (!(Number.isSafeInteger(count) && count >= lengthMin && count <= lengthMax))
		throw new RangeError('Count should be ' + lengthMin + ' .. ' + lengthMax);
	let res;
	if (key) {
		res = shm.get(key, count, shm.IPC_CREAT|shm.IPC_EXCL|perm, 0, type);
	} else {
		do {
			key = _keyGen();
			res = shm.get(key, count, shm.IPC_CREAT|shm.IPC_EXCL|perm, 0, type);
		} while(!res);
	}
	if (res) {
		res.key = key;
	}
	return res;
}

/**
 * Create POSIX shared memory object
 * @param {string} name - string name of shared memory object, should start with '/'
 *  Eg. '/test' will create virtual file '/dev/shm/test' in tmpfs for Linix
 * @param {int} count - number of elements
 * @param {string} typeKey - see keys of BufferType
 * @param {string} permStr - permissions, default is 660
 * @return {mixed/null} shared memory buffer/array object, or null if already exists with provided name
 *  Class depends on param typeKey: Buffer or descendant of TypedArray
 */
function createPosix(name, count, typeKey /*= 'Buffer'*/, permStr /*= '660'*/) {
	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	if (permStr === undefined || isNaN( Number.parseInt(permStr, 8)))
		permStr = '660';
	const perm = Number.parseInt(permStr, 8);

	const type = BufferType[typeKey];
	//var size1 = BufferTypeSizeof[typeKey];
	//var size = size1 * count;
	if (!(Number.isSafeInteger(count) && count >= lengthMin && count <= lengthMax))
		throw new RangeError('Count should be ' + lengthMin + ' .. ' + lengthMax);
	const oflag = shm.O_CREAT | shm.O_RDWR | shm.O_EXCL;
	const mmap_flags = shm.MAP_SHARED;
	const res = shm.getPosix(name, count, oflag, perm, mmap_flags, type);

	return res;
}

/**
 * Get System V/POSIX shared memory
 * @param {int/string} key - integer key of System V shared memory segment, or string name of POSIX shared memory object
 * @param {string} typeKey - see keys of BufferType
 * @return {mixed/null} shared memory buffer/array object, see create(), or null if not exists
 */
function get(key, typeKey /*= 'Buffer'*/) {
	if (typeof key === 'string') {
		return getPosix(key, typeKey);
	}
	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	var type = BufferType[typeKey];
	if (!(Number.isSafeInteger(key) && key >= keyMin && key <= keyMax))
		throw new RangeError('Shm key should be ' + keyMin + ' .. ' + keyMax);
	let res = shm.get(key, 0, 0, 0, type);
	if (res) {
		res.key = key;
	}
	return res;
}

/**
 * Get POSIX shared memory object
 * @param {string} name - string name of shared memory object
 * @param {string} typeKey - see keys of BufferType
 * @return {mixed/null} shared memory buffer/array object, see createPosix(), or null if not exists
 */
function getPosix(name, typeKey /*= 'Buffer'*/) {
	if (typeKey === undefined)
		typeKey = 'Buffer';
	if (BufferType[typeKey] === undefined)
		throw new Error("Unknown type key " + typeKey);
	var type = BufferType[typeKey];
	const oflag = shm.O_RDWR;
	const mmap_flags = shm.MAP_SHARED;
	let res = shm.getPosix(name, 0, oflag, 0, mmap_flags, type);
	return res;
}

/**
 * Detach System V/POSIX shared memory
 * For System V: If there are no other attaches for this segment, it will be destroyed
 * For POSIX: It will be destroyed only if `forceDestroy` is true
 * @param {int/string} key - integer key of System V shared memory segment, or string name of POSIX shared memory object
 * @param {bool} forceDestroy - true to destroy even there are other attaches
 * @return {int} 0 on destroy, or count of left attaches, or -1 if not exists
 */
function detach(key, forceDestroy /*= false*/) {
	if (typeof key === 'string') {
		return detachPosix(key, forceDestroy);
	}
	if (forceDestroy === undefined)
		forceDestroy = false;
	return shm.detach(key, forceDestroy);
}

/**
 * Detach POSIX shared memory object
 * @param {string} name - string name of shared memory object
 * @param {bool} forceDestroy - true to unlink
 * @return {int} 0 on destroy, 1 on detach, -1 if not exists
 */
function detachPosix(name, forceDestroy /*= false*/) {
	if (forceDestroy === undefined)
		forceDestroy = false;
	return shm.detachPosix(name, forceDestroy);
}

/**
 * Destroy System V/POSIX shared memory
 * @param {int/string} key - integer key of System V shared memory segment, or string name of POSIX shared memory object
 * @return {boolean}
 */
function destroy(key) {
	return detach(key, true) == 0;
}

/**
 * Detach all created and getted shared memory objects (both System V and POSIX)
 * Will be automatically called on process exit/termination
 * @return {int} count of destroyed System V segments
 */
function detachAll() {
	return shm.detachAll();
}

function _keyGen() {
	return keyMin + Math.floor(Math.random() * keyMax);
}

//Exports
module.exports.create = create;
module.exports.createPosix = createPosix;
module.exports.get = get;
module.exports.getPosix = getPosix;
module.exports.detach = detach;
module.exports.detachPosix = detachPosix;
module.exports.destroy = destroy;
module.exports.detachAll = detachAll;
module.exports.getTotalSize = shm.getTotalUsedSize;
module.exports.getTotalCreatedSize = shm.getTotalAllocatedSize;
module.exports.BufferType = BufferType;
module.exports.LengthMax = lengthMax;
