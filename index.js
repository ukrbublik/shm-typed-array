'use strict';
const shm=require('./build/Release/shm.node');
const uintMax=Math.pow(2,32)-1;
const keyMin=1;
const keyMax=uintMax-keyMin;
const perm=Number.parseInt('660',8);

module.exports.create=create;
module.exports.get=get;

function create(size){
	if(!(Number.isSafeInteger(size) && size>0 && size<=uintMax))
		throw new RangeError('size 必須為整數，且在 1 ~ '+uintMax+' 範圍內');
	let key,res;
	do{
		key=keyGen();
		res=shm.get(key,size,shm.IPC_CREAT|shm.IPC_EXCL|perm,0);
	}while(!res);
	res.key=key;
	return res;
}
function get(key){
	if(!(Number.isSafeInteger(key) && key>0 && key<=uintMax))
		throw new RangeError('key 必須為整數，且在 1 ~ '+uintMax+' 範圍內');
	let res=shm.get(key,0,0);
	res.key=key;
	return res;
}

function keyGen(){
	return keyMin+Math.floor(Math.random()*keyMax);
}