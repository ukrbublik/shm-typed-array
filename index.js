'use strict';
const shm = require('./build/Release/shm.node');
const nodeCleanup = require('node-cleanup');

const uint32Max = Math.pow(2,32) - 1;
const keyMin = 1;
const keyMax = uint32Max - keyMin;
const perm = Number.parseInt('660', 8);
const sizeMin = 1;
// NODE_BUFFER_MAX_LENGTH = 2GB (0x7fffffff) for 64bit, 1GB for 32bit
const sizeMax = NODE_BUFFER_MAX_LENGTH;

module.exports.create = create;
module.exports.get = get;
module.exports.destroy = destroy;
module.exports.destroyAll = destroyAll;

process.on('exit', shm.destroyAll);
nodeCleanup(function () {
	try {
		shm.destroyAll();
	} catch(exc) { console.error(exc); }
});

function create(size) {
	if (!(Number.isSafeInteger(size) && size >= sizeMin && size <= sizeMax))
		throw new RangeError('shm size should be ' + sizeMin + ' .. ' + sizeMax);
	let key, res;
	do {
		key = keyGen();
		res = shm.get(key, size, shm.IPC_CREAT|shm.IPC_EXCL|perm, 0);
	} while(!res);
	res.key = key;
	return res;
}

function get(key) {
	if (!(Number.isSafeInteger(key) && key >= keyMin && key <= keyMax))
		throw new RangeError('shm key should be ' + keyMin + ' .. ' + keyMax);
	let res = shm.get(key, 0, 0);
	res.key = key;
	return res;
}

function keyGen() {
	return keyMin + Math.floor(Math.random() * keyMax);
}

function destroyAll() {
	shm.destroyAll();
}
