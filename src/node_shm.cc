#include "node_shm.h"

//-------------------------------

namespace node {
namespace Buffer {

	using v8::ArrayBuffer;
	using v8::ArrayBufferCreationMode;
	using v8::EscapableHandleScope;
	using v8::Isolate;
	using v8::Local;
	using v8::MaybeLocal;
	using v8::Object;

	//using v8::Context;
	//using v8::FunctionCallbackInfo;
	//using v8::Integer;
	//using v8::Maybe;
	//using v8::Persistent;
	//using v8::String;
	//using v8::Uint32Array;
	//using v8::Uint8Array;
	//using v8::Value;
	//using v8::WeakCallbackInfo;

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t length
		//todo: not only float32
	) {
		EscapableHandleScope scope(isolate);

		if (length > node::Buffer::kMaxLength)
			return Local<Object>();

		Local<Object> obj;
		Local<ArrayBuffer> ab = ArrayBuffer::New(
			isolate,
			data,
			length,
			ArrayBufferCreationMode::kInternalized);
		if (data == nullptr)
			ab->Neuter();
		Local<Float32Array> ui = Float32Array::New(ab, 0, length);

		if (true)
			return scope.Escape(ui);

		return Local<Object>();
	}

}
}

//-------------------------------

namespace Nan {

	inline MaybeLocal<Object> NewTypedBuffer(
	      char *data
	    , size_t length
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
	  ) {
	    // arbitrary buffer lengths requires
	    // NODE_MODULE_VERSION >= IOJS_3_0_MODULE_VERSION
	    //assert(length <= imp::kMaxLength && "too large buffer");
	    assert(length <= node::Buffer::kMaxLength && "too large buffer");

		if (1) { //float32
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, length);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, length));
			#endif
	    } else { //uint8
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::New(
			        Isolate::GetCurrent(), data, length, callback, hint);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::New(
			        Isolate::GetCurrent(), data, length, callback, hint));
			#endif
	    }

	  }

}

//-------------------------------

namespace node {
namespace node_shm {

	using node::AtExit;
	using v8::Handle;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::Value;

	// Arrays to keep info about created segments, call it "info ararys"
	int shmSegmentsLen = 0;
	int shmSegmentsLenMax = 0;
	int* shmSegmentsIds = NULL;
	void** shmSegmentsAddrs = NULL;

	// Declare private methods
	static void destroyShmSegments();
	static void initShmSegmentsInfo();
	static void destroyShmSegment(int resId, void* addr);
	static void addShmSegmentInfo(int resId, void* addr);
	static void removeShmSegmentInfo(int resId);
	static void FreeCallback(char* data, void* hint);
	static void Init(Handle<Object> target);
	static void AtNodeExit(void*);


	// Init info arrays
	static void initShmSegmentsInfo() {
		destroyShmSegments();

		shmSegmentsLen = 0;
		shmSegmentsLenMax = 16; //will be multiplied by 2 when arrays are full
		shmSegmentsIds = new int[shmSegmentsLenMax];
		shmSegmentsAddrs = new void*[shmSegmentsLenMax];
	}

	// Destroy all segments and delete info arrays
	static void destroyShmSegments() {
		if (shmSegmentsLen > 0) {
			void* addr;
			int resId;
			for (int i = 0 ; i < shmSegmentsLen ; i++) {
				addr = shmSegmentsAddrs[i];
				resId = shmSegmentsIds[i];
				destroyShmSegment(resId, addr);
			}
		}

		SAFE_DELETE_ARR(shmSegmentsIds);
		SAFE_DELETE_ARR(shmSegmentsAddrs);
		shmSegmentsLen = 0;
	}

	// Add segment to info arrays
	static void addShmSegmentInfo(int resId, void* addr) {
		int* newShmSegmentsIds;
		void** newShmSegmentsAddrs;
		if (shmSegmentsLen == shmSegmentsLenMax) {
			//extend ararys by *2 when full
			shmSegmentsLenMax *= 2;
			newShmSegmentsIds = new int[shmSegmentsLenMax];
			newShmSegmentsAddrs = new void*[shmSegmentsLenMax];
			std::copy(shmSegmentsIds, shmSegmentsIds + shmSegmentsLen, newShmSegmentsIds);
			std::copy(shmSegmentsAddrs, shmSegmentsAddrs + shmSegmentsLen, newShmSegmentsAddrs);
			delete [] shmSegmentsIds;
			delete [] shmSegmentsAddrs;
			shmSegmentsIds = newShmSegmentsIds;
			shmSegmentsAddrs = newShmSegmentsAddrs;
		}
		shmSegmentsIds[shmSegmentsLen] = resId;
		shmSegmentsAddrs[shmSegmentsLen] = addr;
		shmSegmentsLen++;
	}

	// Remove segment from info arrays
	static void removeShmSegmentInfo(int resId) {
		int* end = shmSegmentsIds + shmSegmentsLen;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsLen, resId);
		if (found == end)
			return; //not found
		int i = found - shmSegmentsIds;
		shmSegmentsLen--;
		if (i == shmSegmentsLen-1) {
			//removing last element
		} else {
			std::copy(shmSegmentsIds + i + 1, 
				shmSegmentsIds + shmSegmentsLen, 
				shmSegmentsIds + i);
			std::copy(shmSegmentsAddrs + i + 1, 
				shmSegmentsAddrs + shmSegmentsLen,
				shmSegmentsAddrs + i);
		}
		shmSegmentsIds[shmSegmentsLen-1] = 0;
		shmSegmentsAddrs[shmSegmentsLen-1] = NULL;
	}

	// Destroy segment
	static void destroyShmSegment(int resId, void* addr) {
		int err;
		struct shmid_ds shmid_ds;
		//detach
		shmdt(addr);
		//destroy if there are no more attaches
		err = shmctl(resId, IPC_STAT, &shmid_ds);
		if (err == 0 && shmid_ds.shm_nattch == 0)
			shmctl(resId, IPC_RMID, 0);
	}

	// Used only when creating byte-array (Buffer), not typed array
	// Because impl of CallbackInfo::New() is not public (see https://github.com/nodejs/node/blob/v6.x/src/node_buffer.cc)
	// Developer can destroy shared memory blocks manually by shm.destroy()
	// Also shm.destroyAll() will be called on process termination
	static void FreeCallback(char* data, void* hint) {
		int resId = reinterpret_cast<intptr_t>(hint);
		void* addr = (void*) data;

		destroyShmSegment(resId, addr);
		removeShmSegmentInfo(resId);
	}

	// Create or get shared memory
	// Params:
	//  key_t key
	//  size_t size
	//  int shmflg - flags for shmget()
	//  int at_shmflg - flags for shmat()
	//  todo typed, [][] ????
	NAN_METHOD(get) {
		Nan::HandleScope scope;
		int err;
		extern int errno;
		struct shmid_ds shmid_ds;
		key_t key = info[0]->Uint32Value();
		size_t size = info[1]->Uint32Value();
		int shmflg = info[2]->Uint32Value();
		int at_shmflg = info[3]->Uint32Value();
		
		int resId = shmget(key, size, shmflg);
		if (resId == -1) {
			switch(errno) {
				case EEXIST: // already exists
				case EIDRM:  // scheduled for deletion
				case ENOENT: // not exists
					info.GetReturnValue().SetNull();
					return;
				case EINVAL: // should be SHMMIN <= size <= SHMMAX
					return Nan::ThrowRangeError(strerror(errno));
				default:
					return Nan::ThrowError(strerror(errno));
			}
		} else {
			if (size == 0) {
				err = shmctl(resId, IPC_STAT, &shmid_ds);
				if (err == 0)
					size = shmid_ds.shm_segsz;
			}
			
			void* res = shmat(resId, NULL, at_shmflg);
			if (res == (void *)-1)
				return Nan::ThrowError(strerror(errno));

			addShmSegmentInfo(resId, res);

			info.GetReturnValue().Set(Nan::NewTypedBuffer(
				reinterpret_cast<char*>(res),
				size,
				FreeCallback,
				reinterpret_cast<void*>(static_cast<intptr_t>(resId))
			).ToLocalChecked());
		}
	}

	// Destroy shared memory segment
	// Params:
	//  key_t key
	NAN_METHOD(destroy) {
		Nan::HandleScope scope;
		key_t key = info[0]->Uint32Value();

		int resId = shmget(key, 0, 0);
		if (resId == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					return;
				default:
					return Nan::ThrowError(strerror(errno));
			}
		} else {
			int* end = shmSegmentsIds + shmSegmentsLen;
			int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsLen, resId);
			if (found == end)
				return; //not found
			int i = found - shmSegmentsIds;
			void* addr = shmSegmentsAddrs[i];

			destroyShmSegment(resId, addr);
			removeShmSegmentInfo(resId);
		}
	}

	// Destroy all created shared memory segments
	NAN_METHOD(destroyAll) {
		destroyShmSegments();
		initShmSegmentsInfo();
	}

	// node::AtExit
	static void AtNodeExit(void*) {
		destroyShmSegments();
	}

	// Init module
	static void Init(Handle<Object> target) {
		initShmSegmentsInfo();
		
		Nan::SetMethod(target, "get", get);
		Nan::SetMethod(target, "destroy", destroy);
		Nan::SetMethod(target, "destroyAll", destroyAll);

		target->Set(Nan::New("IPC_PRIVATE").ToLocalChecked(), Nan::New<Number>(IPC_PRIVATE));
		target->Set(Nan::New("IPC_CREAT").ToLocalChecked(), Nan::New<Number>(IPC_CREAT));
		target->Set(Nan::New("IPC_EXCL").ToLocalChecked(), Nan::New<Number>(IPC_EXCL));
		target->Set(Nan::New("SHM_RDONLY").ToLocalChecked(), Nan::New<Number>(SHM_RDONLY));
		target->Set(Nan::New("NODE_BUFFER_MAX_LENGTH").ToLocalChecked(), Nan::New<Number>(node::Buffer::kMaxLength));
		
		node::AtExit(AtNodeExit);
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
