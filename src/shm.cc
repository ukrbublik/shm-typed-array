#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "nan.h"
#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

namespace node {
namespace node_shm {

using node::AtExit;
using v8::Handle;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Value;

int exitCheckLen=0;
int exitCheckMax;
int* exitCheckResId;
void** exitCheckAddr;

static void addExitCheck(int resId,void* addr){
	int* extExitCheckResId;
	void** extExitCheckAddr;
	if(exitCheckLen==exitCheckMax){
		exitCheckMax*=2;
		extExitCheckResId=new int[exitCheckMax];
		extExitCheckAddr=new void*[exitCheckMax];
		std::copy(exitCheckResId,exitCheckResId+exitCheckLen,extExitCheckResId);
		std::copy(exitCheckAddr,exitCheckAddr+exitCheckLen,extExitCheckAddr);
		delete [] exitCheckResId;
		delete [] exitCheckAddr;
		exitCheckResId=extExitCheckResId;
		exitCheckAddr=extExitCheckAddr;
	}
	exitCheckResId[exitCheckLen]=resId;
	exitCheckAddr[exitCheckLen]=addr;
	exitCheckLen++;
}
static void delExitCheck(int resId){
	int* tmp=exitCheckResId+exitCheckLen;
	int* result=std::find(exitCheckResId,exitCheckResId+exitCheckLen,resId);
	
	if(result==tmp) return;//沒找到
	exitCheckLen--;
	if(result==tmp-1) return;//在最後找到
	std::copy(result+1,exitCheckResId+exitCheckLen,exitCheckResId);
	std::copy(exitCheckAddr+(result-exitCheckResId)+1,exitCheckAddr+exitCheckLen,exitCheckAddr);
}
static void exitCheck(void*){
	int err;
	struct shmid_ds shmid_ds;
	int checkAt=0;
	while(checkAt<exitCheckLen){
		shmdt(exitCheckAddr[checkAt]);
		err=shmctl(exitCheckResId[checkAt],IPC_STAT,&shmid_ds);
		if(err==0 && shmid_ds.shm_nattch==0)
			shmctl(exitCheckResId[checkAt],IPC_RMID,0);
		checkAt++;
	}
}
static void FreeCallback(char* data,void* hint){
	int err;
	struct shmid_ds shmid_ds;
	int resId=reinterpret_cast<intptr_t>(hint);
	
	shmdt(data);
	err=shmctl(resId,IPC_STAT,&shmid_ds);
	
	if(err==0 && shmid_ds.shm_nattch==0)
		shmctl(resId,IPC_RMID,0);
	delExitCheck(resId);
}

NAN_METHOD(get){
	Nan::HandleScope scope;

	int err;
	extern int errno;
	struct shmid_ds shmid_ds;
	key_t key = info[0]->Uint32Value();
	size_t size = info[1]->Uint32Value();
	int shmflg = info[2]->Uint32Value();
	int at_shmflg = info[3]->Uint32Value();
	
	int resId=shmget(key,size,shmflg);
	if(resId==-1){
		switch(errno){
			case EEXIST: //進行新增但 Key 已存在
			case EIDRM: //目標已被刪除
			case ENOENT: //不存在且不進行新增
				info.GetReturnValue().SetNull();
				return;
			case EINVAL: return Nan::ThrowRangeError(strerror(errno));
			default:
				return Nan::ThrowError(strerror(errno));
		}
	}
	if(size==0){
		err=shmctl(resId,IPC_STAT,&shmid_ds);
		if(err==0)
			size=shmid_ds.shm_segsz;
	}
	
	void* res=shmat(resId,NULL,at_shmflg);
	if(res==(void *)-1)
		return Nan::ThrowError(strerror(errno));
	addExitCheck(resId,res);

	info.GetReturnValue().Set(Nan::NewBuffer(
		reinterpret_cast<char*>(res),
		size,
		FreeCallback,
		reinterpret_cast<void*>(static_cast<intptr_t>(resId))
	).ToLocalChecked());
}

static void Init(Handle<Object> target){
	exitCheckMax=16;
	exitCheckResId=new int[exitCheckMax];
	exitCheckAddr=new void*[exitCheckMax];
	
	Nan::SetMethod(target,"get",get);

	target->Set(Nan::New("IPC_PRIVATE").ToLocalChecked(),Nan::New<Number>(IPC_PRIVATE));

	target->Set(Nan::New("IPC_CREAT").ToLocalChecked(),Nan::New<Number>(IPC_CREAT));
	target->Set(Nan::New("IPC_EXCL").ToLocalChecked(),Nan::New<Number>(IPC_EXCL));

	target->Set(Nan::New("SHM_RDONLY").ToLocalChecked(),Nan::New<Number>(SHM_RDONLY));
	
	AtExit(exitCheck);
}

}
}

NODE_MODULE(shm, node::node_shm::Init);