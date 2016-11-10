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
	using v8::Integer;
	using v8::Maybe;
	using v8::String;
	using v8::Value;
	using v8::Int8Array;
	using v8::Uint8Array;
	using v8::Uint8ClampedArray;
	using v8::Int16Array;
	using v8::Uint16Array;
	using v8::Int32Array;
	using v8::Uint32Array;
	using v8::Float32Array;
	using v8::Float64Array;

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t length,
		TypedArrayType type = TA_FLOAT64
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

		Local<Object> ui;
		switch(type) {
		case TA_INT8:
			ui = Int8Array::New(ab, 0, length);
		break;
		case TA_UINT8:
			ui = Uint8Array::New(ab, 0, length);
		break;
		case TA_UINT8CLAMPED:
			ui = Uint8ClampedArray::New(ab, 0, length);
		break;
		case TA_INT16:
			ui = Int16Array::New(ab, 0, length);
		break;
		case TA_UINT16:
			ui = Uint16Array::New(ab, 0, length);
		break;
		case TA_INT32:
			ui = Int32Array::New(ab, 0, length);
		break;
		case TA_UINT32:
			ui = Uint32Array::New(ab, 0, length);
		break;
		case TA_FLOAT32:
			ui = Float32Array::New(ab, 0, length);
		break;
		default:
		case TA_FLOAT64:
			ui = Float64Array::New(ab, 0, length);
		break;
		}

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
	    , TypedArrayType type = TA_NONE
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback = NULL
	#else
	    , node::smalloc::FreeCallback callback = NULL
	#endif
	    , void *hint = NULL
	  ) {
	    // arbitrary buffer lengths requires
	    // NODE_MODULE_VERSION >= IOJS_3_0_MODULE_VERSION
	    //assert(length <= imp::kMaxLength && "too large buffer");
	    assert(length <= node::Buffer::kMaxLength && "too large buffer");

		if (type != TA_NONE) {
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, length, type);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, length, type));
			#endif
	    } else {
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
	int shmSegmentsCnt = 0;
	int shmSegmentsCntMax = 0;
	int* shmSegmentsIds = NULL;
	void** shmSegmentsAddrs = NULL;

	// Declare private methods
	static int destroyShmSegments();
	static void initShmSegmentsInfo();
	static int destroyShmSegment(int resId, void* addr, bool force = false);
	static void addShmSegmentInfo(int resId, void* addr);
	static bool removeShmSegmentInfo(int resId);
	static void FreeCallback(char* data, void* hint);
	static void Init(Handle<Object> target);
	static void AtNodeExit(void*);


	// Init info arrays
	static void initShmSegmentsInfo() {
		destroyShmSegments();

		shmSegmentsCnt = 0;
		shmSegmentsCntMax = 16; //will be multiplied by 2 when arrays are full
		shmSegmentsIds = new int[shmSegmentsCntMax];
		shmSegmentsAddrs = new void*[shmSegmentsCntMax];
	}

	// Destroy all segments and delete info arrays
	// Returns count of detached blocks
	static int destroyShmSegments() {
		int res = 0;
		if (shmSegmentsCnt > 0) {
			void* addr;
			int resId;
			for (int i = 0 ; i < shmSegmentsCnt ; i++) {
				addr = shmSegmentsAddrs[i];
				resId = shmSegmentsIds[i];
				if (destroyShmSegment(resId, addr) != -1)
					res++;
			}
		}

		SAFE_DELETE_ARR(shmSegmentsIds);
		SAFE_DELETE_ARR(shmSegmentsAddrs);
		shmSegmentsCnt = 0;
		return res;
	}

	// Add segment to info arrays
	static void addShmSegmentInfo(int resId, void* addr) {
		int* newShmSegmentsIds;
		void** newShmSegmentsAddrs;
		if (shmSegmentsCnt == shmSegmentsCntMax) {
			//extend ararys by *2 when full
			shmSegmentsCntMax *= 2;
			newShmSegmentsIds = new int[shmSegmentsCntMax];
			newShmSegmentsAddrs = new void*[shmSegmentsCntMax];
			std::copy(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, newShmSegmentsIds);
			std::copy(shmSegmentsAddrs, shmSegmentsAddrs + shmSegmentsCnt, newShmSegmentsAddrs);
			delete [] shmSegmentsIds;
			delete [] shmSegmentsAddrs;
			shmSegmentsIds = newShmSegmentsIds;
			shmSegmentsAddrs = newShmSegmentsAddrs;
		}
		shmSegmentsIds[shmSegmentsCnt] = resId;
		shmSegmentsAddrs[shmSegmentsCnt] = addr;
		shmSegmentsCnt++;
	}

	// Remove segment from info arrays
	static bool removeShmSegmentInfo(int resId) {
		int* end = shmSegmentsIds + shmSegmentsCnt;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
		if (found == end)
			return false; //not found
		int i = found - shmSegmentsIds;
		if (i == shmSegmentsCnt-1) {
			//removing last element
		} else {
			std::copy(shmSegmentsIds + i + 1, 
				shmSegmentsIds + shmSegmentsCnt, 
				shmSegmentsIds + i);
			std::copy(shmSegmentsAddrs + i + 1, 
				shmSegmentsAddrs + shmSegmentsCnt,
				shmSegmentsAddrs + i);
		}
		shmSegmentsIds[shmSegmentsCnt-1] = 0;
		shmSegmentsAddrs[shmSegmentsCnt-1] = NULL;
		shmSegmentsCnt--;
		return true;
	}

	// Destroy segment
	// Returns count of left attaches or -1 on error
	static int destroyShmSegment(int resId, void* addr, bool force /*= false*/) {
		int err;
		struct shmid_ds shmid_ds;
		//detach
		err = shmdt(addr);
		if (err == 0) {
			//get stat
			err = shmctl(resId, IPC_STAT, &shmid_ds);
			if (err == 0) {
				//destroy if there are no more attaches or force==true
				if (force || shmid_ds.shm_nattch == 0) {
					err = shmctl(resId, IPC_RMID, 0);
					if (err == 0) {
						return 0; //detached and destroyed
					} else
						{} //Nan::ThrowError(strerror(errno));
				} else
					return shmid_ds.shm_nattch; //detached, but not destroyed
			} else
				{} //Nan::ThrowError(strerror(errno));
		} else {
			switch(errno) {
				case EINVAL: // wrong addr
				default:
					{} //Nan::ThrowError(strerror(errno));
			}
		}
		return -1;
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
	//  enum TypedArrayType type
	// Returns buffer or typed array, depends on input param type
	NAN_METHOD(get) {
		Nan::HandleScope scope;
		int err;
		extern int errno;
		struct shmid_ds shmid_ds;
		key_t key = info[0]->Uint32Value();
		size_t size = info[1]->Uint32Value();
		int shmflg = info[2]->Uint32Value();
		int at_shmflg = info[3]->Uint32Value();
		TypedArrayType type = (TypedArrayType) info[4]->Int32Value();
		
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
				else
					return Nan::ThrowError(strerror(errno));
			}
			
			void* res = shmat(resId, NULL, at_shmflg);
			if (res == (void *)-1)
				return Nan::ThrowError(strerror(errno));

			addShmSegmentInfo(resId, res);

			info.GetReturnValue().Set(Nan::NewTypedBuffer(
				reinterpret_cast<char*>(res),
				size,
				type,
				FreeCallback,
				reinterpret_cast<void*>(static_cast<intptr_t>(resId))
			).ToLocalChecked());
		}
	}

	// Destroy shared memory segment
	// Params:
	//  key_t key
	//  bool force
	// Returns count of left attaches or -1 on error
	NAN_METHOD(destroy) {
		Nan::HandleScope scope;
		key_t key = info[0]->Uint32Value();
		bool force = info[1]->BooleanValue();

		int resId = shmget(key, 0, 0);
		if (resId == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				default:
					return Nan::ThrowError(strerror(errno));
			}
		} else {
			int* end = shmSegmentsIds + shmSegmentsCnt;
			int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
			if (found == end) {
				//not found in info array
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
			int i = found - shmSegmentsIds;
			void* addr = shmSegmentsAddrs[i];

			int res = destroyShmSegment(resId, addr, force);
			if (res != -1)
				removeShmSegmentInfo(resId);
			info.GetReturnValue().Set(Nan::New<Number>(res));
		}
	}

	// Destroy all created shared memory segments
	// Returns count of detached blocks
	NAN_METHOD(destroyAll) {
		int cnt = destroyShmSegments();
		initShmSegmentsInfo();
		info.GetReturnValue().Set(Nan::New<Number>(cnt));
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
		//enum TypedArrayType
		target->Set(Nan::New("TA_NONE").ToLocalChecked(), Nan::New<Number>(TA_NONE));
		target->Set(Nan::New("TA_INT8").ToLocalChecked(), Nan::New<Number>(TA_INT8));
		target->Set(Nan::New("TA_UINT8").ToLocalChecked(), Nan::New<Number>(TA_UINT8));
		target->Set(Nan::New("TA_UINT8CLAMPED").ToLocalChecked(), Nan::New<Number>(TA_UINT8CLAMPED));
		target->Set(Nan::New("TA_INT16").ToLocalChecked(), Nan::New<Number>(TA_INT16));
		target->Set(Nan::New("TA_UINT16").ToLocalChecked(), Nan::New<Number>(TA_UINT16));
		target->Set(Nan::New("TA_INT32").ToLocalChecked(), Nan::New<Number>(TA_INT32));
		target->Set(Nan::New("TA_UINT32").ToLocalChecked(), Nan::New<Number>(TA_UINT32));
		target->Set(Nan::New("TA_FLOAT32").ToLocalChecked(), Nan::New<Number>(TA_FLOAT32));
		target->Set(Nan::New("TA_FLOAT64").ToLocalChecked(), Nan::New<Number>(TA_FLOAT64));

		node::AtExit(AtNodeExit);
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
