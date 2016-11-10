'use strict';
const shm = require('./build/Release/shm.node');
const nodeCleanup = require('node-cleanup');
const cluster = require('cluster');

const uint32Max = Math.pow(2,32) - 1;
const keyMin = 1;
const keyMax = uint32Max - keyMin;
const perm = Number.parseInt('660', 8);
const sizeMin = 1;
const sizeMax = shm.NODE_BUFFER_MAX_LENGTH;

const cleanup = function () {
	try {
		var cnt = shm.destroyAll();
		if (cnt > 0)
			console.info('shm blocks detached:', cnt);
	} catch(exc) { console.error(exc); }
};
if (cluster.isMaster || true) {
	process.on('exit', cleanup);
	nodeCleanup(cleanup);
}

/**
 * Types 
 */
const TypedArrayType = {
	'Buffer': shm.TA_NONE,
	'Int8Array': shm.TA_INT8,
	'Uint8Array': shm.TA_UINT8,
	'Uint8ClampedArray': shm.TA_UINT8CLAMPED,
	'Int16Array': shm.TA_INT16,
	'Uint16Array': shm.TA_UINT16,
	'Int32Array': shm.TA_INT32,
	'Uint32Array': shm.TA_UINT32,
	'Float32Array': shm.TA_FLOAT32, 
	'Float64Array': shm.TA_FLOAT64,
};

/**
 * Create shared memory
 * @param int size - size in bytes
 * @param int type - see TypedArrayType
 * @return shared memory buffer/array object
 *  Type depends on type: Buffer (if type == TA_NONE) or TypedArray
 *  Has property 'key' - integer key of created shared memory block
 */
function create(size, type) {
	if (type === undefined)
		type = TypedArrayType.Buffer;
	if (!(Number.isSafeInteger(size) && size >= sizeMin && size <= sizeMax))
		throw new RangeError('shm size should be ' + sizeMin + ' .. ' + sizeMax);
	let key, res;
	do {
		key = _keyGen();
		res = shm.get(key, size, shm.IPC_CREAT|shm.IPC_EXCL|perm, 0, type);
	} while(!res);
	res.key = key;
	return res;
}

/**
 * Get shared memory
 * @param int key - integer key of shared memory block
 * @param int type - see TypedArrayType
 * @return shared memory buffer/array object, see create()
 */
function get(key, type) {
	if (!(Number.isSafeInteger(key) && key >= keyMin && key <= keyMax))
		throw new RangeError('shm key should be ' + keyMin + ' .. ' + keyMax);
	let res = shm.get(key, 0, 0, 0, type);
	res.key = key;
	return res;
}

/**
 * Destroy shared memory
 * @param int key - integer key of shared memory block
 * @param bool force - true to destroy even there are other processed uses this segment
 * @return int count of left attaches or -1 on error
 */
function destroy(key, force) {
	if (force === undefined)
		force = false;
	return shm.destroy(key, force);
}

/**
 * Destroy all created  shared memory blocks
 * Will be automatically called on process exit/termination
 * @return int count of detached blocks
 */
function destroyAll() {
	return shm.destroyAll();
}

function _keyGen() {
	return keyMin + Math.floor(Math.random() * keyMax);
}

//Exports
module.exports.create = create;
module.exports.get = get;
module.exports.destroy = destroy;
module.exports.destroyAll = destroyAll;
module.exports.TypedArrayType = TypedArrayType;
module.exports.sizeMax = sizeMax; //2GB (0x7fffffff) for 64bit, 1GB for 32bit
