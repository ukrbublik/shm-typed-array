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
		size_t count
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
		, ShmBufferType type
	) {
		size_t length = count * getSize1ForShmBufferType(type);

		EscapableHandleScope scope(isolate);

		/*
		MaybeLocal<Object> mlarr = node::Buffer::New(
			isolate, data, length, callback, hint);
		Local<Object> larr = mlarr.ToLocalChecked();
		Uint8Array* arr = (Uint8Array*) *larr;
		Local<ArrayBuffer> ab = arr->Buffer();
		*/

		Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, data, length, 
			ArrayBufferCreationMode::kExternalized);
		
		Local<Object> ui;
		switch(type) {
			case SHMBT_INT8:
				ui = Int8Array::New(ab, 0, count);
			break;
			case SHMBT_UINT8:
				ui = Uint8Array::New(ab, 0, count);
			break;
			case SHMBT_UINT8CLAMPED:
				ui = Uint8ClampedArray::New(ab, 0, count);
			break;
			case SHMBT_INT16:
				ui = Int16Array::New(ab, 0, count);
			break;
			case SHMBT_UINT16:
				ui = Uint16Array::New(ab, 0, count);
			break;
			case SHMBT_INT32:
				ui = Int32Array::New(ab, 0, count);
			break;
			case SHMBT_UINT32:
				ui = Uint32Array::New(ab, 0, count);
			break;
			case SHMBT_FLOAT32:
				ui = Float32Array::New(ab, 0, count);
			break;
			default:
			case SHMBT_FLOAT64:
				ui = Float64Array::New(ab, 0, count);
			break;
		}

		return scope.Escape(ui);
	}

}
}

//-------------------------------

namespace Nan {

	inline MaybeLocal<Object> NewTypedBuffer(
	      char *data
	    , size_t count
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
	    , ShmBufferType type
	) {
		size_t length = count * getSize1ForShmBufferType(type);

		if (type != SHMBT_BUFFER) {
	  	assert(count <= node::Buffer::kMaxLength && "too large typed buffer");
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, count, callback, hint, type);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, count, callback, hint, type));
			#endif
	  } else {
	  	assert(length <= node::Buffer::kMaxLength && "too large buffer");
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
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::Value;

	// Arrays to keep info about created segments, call it "info ararys"
	int shmSegmentsCnt = 0;
	size_t shmSegmentsBytes = 0;
	int shmSegmentsCntMax = 0;
	int* shmSegmentsIds = NULL;
	void** shmSegmentsAddrs = NULL;

	// Declare private methods
	static int detachShmSegments();
	static void initShmSegmentsInfo();
	static int detachShmSegment(int resId, void* addr, bool force = false, bool onExit = false);
	static void addShmSegmentInfo(int resId, void* addr);
	static bool hasShmSegmentInfo(int resId);
	static bool removeShmSegmentInfo(int resId);
	static void FreeCallback(char* data, void* hint);
	static void Init(Local<Object> target);
	static void AtNodeExit(void*);


	// Init info arrays
	static void initShmSegmentsInfo() {
		detachShmSegments();

		shmSegmentsCnt = 0;
		shmSegmentsCntMax = 16; //will be multiplied by 2 when arrays are full
		shmSegmentsIds = new int[shmSegmentsCntMax];
		shmSegmentsAddrs = new void*[shmSegmentsCntMax];
	}

	// Detach all segments and delete info arrays
	// Returns count of destroyed segments
	static int detachShmSegments() {
		int res = 0;
		if (shmSegmentsCnt > 0) {
			void* addr;
			int resId;
			for (int i = 0 ; i < shmSegmentsCnt ; i++) {
				addr = shmSegmentsAddrs[i];
				resId = shmSegmentsIds[i];
				if (detachShmSegment(resId, addr, false, true) == 0)
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

  static bool hasShmSegmentInfo(int resId) {
		int* end = shmSegmentsIds + shmSegmentsCnt;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
		return (found != end);
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

	// Detach segment
	// Returns count of left attaches or -1 on error
	static int detachShmSegment(int resId, void* addr, bool force, bool onExit) {
		int err;
		struct shmid_ds shminf;
		//detach
		err = shmdt(addr);
		if (err == 0) {
			//get stat
			err = shmctl(resId, IPC_STAT, &shminf);
			if (err == 0) {
				//destroy if there are no more attaches or force==true
				if (force || shminf.shm_nattch == 0) {
					err = shmctl(resId, IPC_RMID, 0);
					if (err == 0) {
						shmSegmentsBytes -= shminf.shm_segsz;
						return 0; //detached and destroyed
					} else {
						if(!onExit)
							Nan::ThrowError(strerror(errno));
					}
				} else {
					return shminf.shm_nattch; //detached, but not destroyed
				}
			} else {
				if(!onExit)
					Nan::ThrowError(strerror(errno));
			}
		} else {
			switch(errno) {
				case EINVAL: // wrong addr
				default:
					if(!onExit)
						Nan::ThrowError(strerror(errno));
				break;
			}
		}
		return -1;
	}

	// Used only when creating byte-array (Buffer), not typed array
	// Because impl of CallbackInfo::New() is not public (see https://github.com/nodejs/node/blob/v6.x/src/node_buffer.cc)
	// Developer can detach shared memory segments manually by shm.detach()
	// Also shm.detachAll() will be called on process termination
	static void FreeCallback(char* data, void* hint) {
		int resId = reinterpret_cast<intptr_t>(hint);
		void* addr = (void*) data;

		detachShmSegment(resId, addr, false, true);
		removeShmSegmentInfo(resId);
	}

	NAN_METHOD(get) {
		Nan::HandleScope scope;
		int err;
		struct shmid_ds shminf;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		size_t count = Nan::To<uint32_t>(info[1]).FromJust();
		int shmflg = Nan::To<uint32_t>(info[2]).FromJust();
		int at_shmflg = Nan::To<uint32_t>(info[3]).FromJust();
		ShmBufferType type = (ShmBufferType) Nan::To<int32_t>(info[4]).FromJust();
		size_t size = count * getSize1ForShmBufferType(type);
		bool isCreate = (size > 0);
		
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
			if (!isCreate) {
				err = shmctl(resId, IPC_STAT, &shminf);
				if (err == 0) {
					size = shminf.shm_segsz;
					count = size / getSize1ForShmBufferType(type);
				} else
					return Nan::ThrowError(strerror(errno));
			}
			
			void* res = shmat(resId, NULL, at_shmflg);
			if (res == (void *)-1)
				return Nan::ThrowError(strerror(errno));

			if (!hasShmSegmentInfo(resId)) {
				addShmSegmentInfo(resId, res);
				shmSegmentsBytes += size;
			}

			info.GetReturnValue().Set(Nan::NewTypedBuffer(
				reinterpret_cast<char*>(res),
				count,
				FreeCallback,
				reinterpret_cast<void*>(static_cast<intptr_t>(resId)),
				type
			).ToLocalChecked());
		}
	}

	NAN_METHOD(detach) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		bool forceDestroy = Nan::To<bool>(info[1]).FromJust();

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

			int res = detachShmSegment(resId, addr, forceDestroy);
			if (res != -1)
				removeShmSegmentInfo(resId);
			info.GetReturnValue().Set(Nan::New<Number>(res));
		}
	}

	NAN_METHOD(detachAll) {
		int cnt = detachShmSegments();
		initShmSegmentsInfo();
		info.GetReturnValue().Set(Nan::New<Number>(cnt));
	}

	NAN_METHOD(getTotalSize) {
		info.GetReturnValue().Set(Nan::New<Number>(shmSegmentsBytes));
	}

	// node::AtExit
	static void AtNodeExit(void*) {
		detachShmSegments();
	}

	// Init module
	static void Init(Local<Object> target) {
		initShmSegmentsInfo();
		
		Nan::SetMethod(target, "get", get);
		Nan::SetMethod(target, "detach", detach);
		Nan::SetMethod(target, "detachAll", detachAll);
		Nan::SetMethod(target, "getTotalSize", getTotalSize);

		Nan::Set(target, Nan::New("IPC_PRIVATE").ToLocalChecked(), Nan::New<Number>(IPC_PRIVATE));
		Nan::Set(target, Nan::New("IPC_CREAT").ToLocalChecked(), Nan::New<Number>(IPC_CREAT));
		Nan::Set(target, Nan::New("IPC_EXCL").ToLocalChecked(), Nan::New<Number>(IPC_EXCL));
		Nan::Set(target, Nan::New("SHM_RDONLY").ToLocalChecked(), Nan::New<Number>(SHM_RDONLY));
		Nan::Set(target, Nan::New("NODE_BUFFER_MAX_LENGTH").ToLocalChecked(), Nan::New<Number>(node::Buffer::kMaxLength));
		//enum ShmBufferType
		Nan::Set(target, Nan::New("SHMBT_BUFFER").ToLocalChecked(), Nan::New<Number>(SHMBT_BUFFER));
		Nan::Set(target, Nan::New("SHMBT_INT8").ToLocalChecked(), Nan::New<Number>(SHMBT_INT8));
		Nan::Set(target, Nan::New("SHMBT_UINT8").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT8));
		Nan::Set(target, Nan::New("SHMBT_UINT8CLAMPED").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT8CLAMPED));
		Nan::Set(target, Nan::New("SHMBT_INT16").ToLocalChecked(), Nan::New<Number>(SHMBT_INT16));
		Nan::Set(target, Nan::New("SHMBT_UINT16").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT16));
		Nan::Set(target, Nan::New("SHMBT_INT32").ToLocalChecked(), Nan::New<Number>(SHMBT_INT32));
		Nan::Set(target, Nan::New("SHMBT_UINT32").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT32));
		Nan::Set(target, Nan::New("SHMBT_FLOAT32").ToLocalChecked(), Nan::New<Number>(SHMBT_FLOAT32));
		Nan::Set(target, Nan::New("SHMBT_FLOAT64").ToLocalChecked(), Nan::New<Number>(SHMBT_FLOAT64));

		node::AtExit(AtNodeExit);
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
