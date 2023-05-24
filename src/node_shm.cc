#include "node_shm.h"
#include "node.h"

//-------------------------------

#if NODE_MODULE_VERSION > NODE_16_0_MODULE_VERSION
namespace {
void emptyBackingStoreDeleter(void*, size_t, void*) {}
}
#endif

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

		#if NODE_MODULE_VERSION > NODE_16_0_MODULE_VERSION
		Local<ArrayBuffer> ab = ArrayBuffer::New(isolate,
			ArrayBuffer::NewBackingStore(data, length, &emptyBackingStoreDeleter, nullptr));
		#else
		Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, data, length,
			ArrayBufferCreationMode::kExternalized);
		#endif

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
	using v8::String;
	using v8::Value;

	enum ShmType {
		SHM_TYPE_SYSTEMV = 0,
		SHM_TYPE_POSIX = 1,
	};

	struct ShmMeta {
		ShmType type;
		int id;
		void* ptr;
		size_t size;
		size_t bufCount;
		ShmBufferType bufType;
		const char* name;
	};

	// Arrays to keep info about created segments, call it "meta ararys"
	int shmSegmentsCnt = 0;
	size_t shmSegmentsBytes = 0;
	int shmSegmentsMetaSize = 0;
	ShmMeta* shmSegmentsMeta = NULL;

	// Declare private methods
	static int detachAllShm();
	static int detachShmSegmentOrObject(ShmMeta meta, bool force = false, bool onExit = false);
	static int detachShmSegment(ShmMeta meta, bool force = false, bool onExit = false);
	static int detachPosixShmObject(ShmMeta meta, bool force = false, bool onExit = false);
	static void initShmSegmentsInfo();
	static void addShmSegmentInfo(ShmMeta meta);
	static bool hasShmSegmentInfo(ShmMeta meta);
	static bool removeShmSegmentInfo(ShmMeta meta);
	static void FreeCallback(char* data, void* hint);
  #if NODE_MODULE_VERSION < NODE_16_0_MODULE_VERSION
	static void Init(Local<Object> target);
  #else
	static void Init(Local<Object> target, Local<Value> module, void* priv);
  #endif
	static void AtNodeExit(void*);


	// Init meta arrays
	static void initShmSegmentsInfo() {
		detachAllShm();

		shmSegmentsCnt = 0;
		shmSegmentsMeta = new ShmMeta[shmSegmentsMetaSize];
		shmSegmentsMetaSize = 16; //will be multiplied by 2 when arrays are full
	}

	// Detach all segments and delete meta arrays
	// Returns count of destroyed segments
	static int detachAllShm() {
		int res = 0;
		if (shmSegmentsCnt > 0) {
			for (int i = 0 ; i < shmSegmentsCnt ; i++) {
				ShmMeta meta = shmSegmentsMeta[i];
				if (detachShmSegmentOrObject(meta, false, true) == 0)
					res++;
			}
		}

		SAFE_DELETE_ARR(shmSegmentsMeta);
		shmSegmentsCnt = 0;
		return res;
	}

	// Add segment to meta arrays
	static void addShmSegmentInfo(ShmMeta meta) {
		ShmMeta* newShmSegmentsData;
		if (shmSegmentsCnt == shmSegmentsMetaSize) {
			//extend ararys by *2 when full
			shmSegmentsMetaSize *= 2;
			newShmSegmentsData = new ShmMeta[shmSegmentsMetaSize];
			std::copy(shmSegmentsMeta, shmSegmentsMeta + shmSegmentsCnt, newShmSegmentsData);
			SAFE_DELETE_ARR(shmSegmentsMeta);
			shmSegmentsMeta = newShmSegmentsData;
		}
		shmSegmentsMeta[shmSegmentsCnt] = meta;
		shmSegmentsCnt++;
	}

  static ShmMeta* findShmSegmentInfo(ShmMeta search) {
		ShmMeta* end = shmSegmentsMeta + shmSegmentsCnt;
		const auto found = std::find_if(shmSegmentsMeta, shmSegmentsMeta + shmSegmentsCnt,
      [&](const auto& el) {
        return el.type == search.type 
					&& (search.id == 0 || el.id == search.id) 
					&& (search.name == NULL || el.name != NULL && strcmp(search.name, el.name) == 0);
			}
    );
		return (found != end ? found : NULL);
  }

  static bool hasShmSegmentInfo(ShmMeta meta) {
		return findShmSegmentInfo(meta) != NULL;
  }

	// Remove segment from meta arrays
	static bool removeShmSegmentInfo(ShmMeta meta) {
		ShmMeta* found = findShmSegmentInfo(meta);
		if (found == NULL)
			return false; //not found
		int i = found - shmSegmentsMeta;
		if (i == shmSegmentsCnt-1) {
			//removing last element
		} else {
			std::copy(shmSegmentsMeta + i + 1,
				shmSegmentsMeta + shmSegmentsCnt,
				shmSegmentsMeta + i);
		}
		SAFE_DELETE_ARR(shmSegmentsMeta[shmSegmentsCnt-1].name);
		shmSegmentsMeta[shmSegmentsCnt-1] = {};
		shmSegmentsCnt--;
		return true;
	}

	// Detach segment or POSIX object
	// Returns 0 if deleted, > 0 if detached, -1 on error
	static int detachShmSegmentOrObject(ShmMeta meta, bool force, bool onExit) {
		if (meta.type == SHM_TYPE_SYSTEMV) {
			return detachShmSegment(meta, force, onExit);
		} else if (meta.type == SHM_TYPE_POSIX) {
			return detachPosixShmObject(meta, force, onExit);
		}
		return -1;
	}

	// Detach segment
	// Returns count of left attaches or -1 on error
	static int detachShmSegment(ShmMeta meta, bool force, bool onExit) {
		int err;
		struct shmid_ds shminf;
		//detach
		err = meta.ptr != NULL ? shmdt(meta.ptr) : 0;
		if (err == 0) {
			meta.ptr = NULL;
			if (meta.id == 0) {
				// meta is obsolete, should be deleted from meta array
				return -2;
			}
			//get stat
			err = shmctl(meta.id, IPC_STAT, &shminf);
			if (err == 0) {
				//destroy if there are no more attaches or force==true
				if (force || shminf.shm_nattch == 0) {
					err = shmctl(meta.id, IPC_RMID, 0);
					if (err == 0) {
						meta.id = 0;
						meta.size = 0;
						shmSegmentsBytes -= shminf.shm_segsz;
						return 0; //detached and destroyed
					} else {
						if (!onExit)
							Nan::ThrowError(strerror(errno));
					}
				} else {
					return shminf.shm_nattch; //detached, but not destroyed
				}
			} else {
				if (!onExit)
					Nan::ThrowError(strerror(errno));
			}
		} else {
			switch(errno) {
				case EINVAL: // wrong addr
				default:
					if (!onExit)
						Nan::ThrowError(strerror(errno));
				break;
			}
		}
		return -1;
	}

	// Detach POSIX object
	// Returns 0 if deleted, 1 if detached, -1 on error
	static int detachPosixShmObject(ShmMeta meta, bool force, bool onExit) {
		int err;
		//detach
		err = meta.ptr != NULL ? munmap(meta.ptr, meta.size) : 0;
		if (err == 0) {
			meta.ptr = NULL;
			if (meta.name == NULL) {
				// meta is obsolete, should be deleted from meta array
				return -2;
			}
			//unlink
			if (force) {
				err = shm_unlink(meta.name);
				if (err == 0) {
					shmSegmentsBytes -= meta.size;
					SAFE_DELETE_ARR(meta.name);
					meta.size = 0;
					return 0; //detached and destroyed
				} else {
					if (!onExit)
						Nan::ThrowError(strerror(errno));
				}
			} else {
				return 1; //detached, but not destroyed
			}
		} else {
			switch(errno) {
				case EINVAL: // wrong addr
				default:
					if (!onExit)
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
		ShmMeta* meta = reinterpret_cast<ShmMeta*>(hint);
		void* addr = (void*) data;

		assert(meta->ptr == addr);
    detachShmSegmentOrObject(*meta, false, true);
		removeShmSegmentInfo(*meta);
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

		int shmid = shmget(key, size, shmflg);
		if (shmid == -1) {
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
				err = shmctl(shmid, IPC_STAT, &shminf);
				if (err == 0) {
					size = shminf.shm_segsz;
					count = size / getSize1ForShmBufferType(type);
				} else
					return Nan::ThrowError(strerror(errno));
			}

			void* res = shmat(shmid, NULL, at_shmflg);
			if (res == (void *)-1)
				return Nan::ThrowError(strerror(errno));

			ShmMeta meta = {SHM_TYPE_SYSTEMV, shmid, res, size, count, type, NULL};
			if (!hasShmSegmentInfo(meta)) {
				addShmSegmentInfo(meta);
				shmSegmentsBytes += size;
			}

			info.GetReturnValue().Set(Nan::NewTypedBuffer(
				reinterpret_cast<char*>(res),
				count,
				FreeCallback,
				reinterpret_cast<void*>(static_cast<ShmMeta*>(&meta)),
				type
			).ToLocalChecked());
		}
	}

	NAN_METHOD(getPosix) {
		Nan::HandleScope scope;
		if (!info[0]->IsString()) {
			return Nan::ThrowTypeError("Argument name must be a string");
		}
		std::string name = (*Nan::Utf8String(info[0]));
		size_t count = Nan::To<uint32_t>(info[1]).FromJust();
		int oflag = Nan::To<uint32_t>(info[2]).FromJust();
		mode_t mode = Nan::To<uint32_t>(info[3]).FromJust();
		int mmap_flags = Nan::To<uint32_t>(info[4]).FromJust();
		ShmBufferType type = (ShmBufferType) Nan::To<int32_t>(info[5]).FromJust();
		size_t size = count * getSize1ForShmBufferType(type);
		bool isCreate = (size > 0);
		size_t realSize = isCreate ? size + sizeof(size) : 0;

		// Create or get shared memory object
		int fd = shm_open(name.c_str(), oflag, mode);
		if (fd == -1) {
			switch(errno) {
				case ENAMETOOLONG: // length of name exceeds PATH_MAX
					return Nan::ThrowRangeError(strerror(errno));
				default:
					return Nan::ThrowError(strerror(errno));
			}
		}

		// Truncate
		int resTrunc;
		if (isCreate) {
			resTrunc = ftruncate(fd, realSize);
			if (resTrunc == -1) {
				switch(errno) {
					case EFBIG: // length exceeds max file size
					case EINVAL: // length exceeds max file size or < 0
						return Nan::ThrowRangeError(strerror(errno));
					default:
						return Nan::ThrowError(strerror(errno));
				}
			}
		}

		// Get size (not accurate, multiplies PAGE_SIZE)
    struct stat sb;
		int resStat;
		if (!isCreate) {
			resStat = fstat(fd, &sb);
			if (resStat == -1) {
				switch(errno) {
					default:
						return Nan::ThrowError(strerror(errno));
				}
			}
			realSize = sb.st_size;
		}
		
		// Map shared memory object
		off_t offset = 0;
		int prot = PROT_READ | PROT_WRITE;
		void* res = mmap(NULL, realSize, prot, mmap_flags, fd, offset);
		if (res == MAP_FAILED) {
			switch(errno) {
				// case EBADF: // not valid fd
				// 	info.GetReturnValue().SetNull();
				// 	return;
				case EINVAL: // length is bad, or flags does not comtain MAP_SHARED / MAP_PRIVATE / MAP_SHARED_VALIDATE
					return Nan::ThrowRangeError(strerror(errno));
				default:
					return Nan::ThrowError(strerror(errno));
			}
		}

		// Read/write actual buffer size at start of shared memory
		size_t* sizePtr = (size_t*) res;
		char* buf = reinterpret_cast<char*>(res);
		buf += sizeof(size);
		if (isCreate) {
			*sizePtr = size;
		} else {
			size = *sizePtr;
			count = size / getSize1ForShmBufferType(type);
		}

		// Write meta
		char* cstr = new char[name.length()+1];
		std::strcpy(cstr, name.c_str());
		ShmMeta meta = {SHM_TYPE_POSIX, 0, res, realSize, count, type, cstr};
		if (!hasShmSegmentInfo(meta)) {
			addShmSegmentInfo(meta);
			shmSegmentsBytes += realSize;
		}

		// Don't save to meta
		close(fd);

		// Build and return buffer
		info.GetReturnValue().Set(Nan::NewTypedBuffer(
			buf,
			count,
			FreeCallback,
			reinterpret_cast<void*>(static_cast<ShmMeta*>(&meta)),
			type
		).ToLocalChecked());
	}

	NAN_METHOD(detach) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		bool forceDestroy = Nan::To<bool>(info[1]).FromJust();

		int shmid = shmget(key, 0, 0);
		if (shmid == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				default:
					return Nan::ThrowError(strerror(errno));
			}
		} else {
		  ShmMeta* found = findShmSegmentInfo({SHM_TYPE_SYSTEMV, shmid, NULL, 0, 0, SHMBT_BUFFER, NULL});
			if (found != NULL) {
				int res = detachShmSegment(*found, forceDestroy);
				if (res != -1)
					removeShmSegmentInfo(*found);
				info.GetReturnValue().Set(Nan::New<Number>(res));
			} else {
				//not found in meta array, means not created by us
				int res = -1;
				if (forceDestroy) {
					ShmMeta meta = {SHM_TYPE_SYSTEMV, shmid, NULL, 0, 0, SHMBT_BUFFER, NULL};
					res = detachShmSegment(meta, forceDestroy);
				}
				info.GetReturnValue().Set(Nan::New<Number>(res));
			}
		}
	}

	NAN_METHOD(detachPosix) {
		Nan::HandleScope scope;
		if (!info[0]->IsString()) {
			return Nan::ThrowTypeError("Argument name must be a string");
		}
		std::string name = (*Nan::Utf8String(info[0]));
		bool forceDestroy = Nan::To<bool>(info[1]).FromJust();

		ShmMeta* found = findShmSegmentInfo({SHM_TYPE_POSIX, NULL, NULL, 0, 0, SHMBT_BUFFER, name.c_str()});
		if (found != NULL) {
			int res = detachPosixShmObject(*found, forceDestroy);
			if (res != -1)
				removeShmSegmentInfo(*found);
			info.GetReturnValue().Set(Nan::New<Number>(res));
		} else {
			//not found in meta array, means not created by us
			int res = -1;
			if (forceDestroy) {
				ShmMeta meta = {SHM_TYPE_POSIX, NULL, NULL, 0, 0, SHMBT_BUFFER, name.c_str()};
				res = detachShmSegment(meta, forceDestroy);
			}
			info.GetReturnValue().Set(Nan::New<Number>(res));
		}
	}

	NAN_METHOD(detachAll) {
		int cnt = detachAllShm();
		initShmSegmentsInfo();
		info.GetReturnValue().Set(Nan::New<Number>(cnt));
	}

	NAN_METHOD(getTotalSize) {
		info.GetReturnValue().Set(Nan::New<Number>(shmSegmentsBytes));
	}

	// node::AtExit
	static void AtNodeExit(void*) {
		detachAllShm();
	}

	// Init module
  #if NODE_MODULE_VERSION < NODE_16_0_MODULE_VERSION
	static void Init(Local<Object> target) {
  #else
	static void Init(Local<Object> target, Local<Value> module, void* priv) {
  #endif
		initShmSegmentsInfo();

		Nan::SetMethod(target, "get", get);
		Nan::SetMethod(target, "getPosix", getPosix);
		Nan::SetMethod(target, "detach", detach);
		Nan::SetMethod(target, "detachPosix", detachPosix);
		Nan::SetMethod(target, "detachAll", detachAll);
		Nan::SetMethod(target, "getTotalSize", getTotalSize);

		Nan::Set(target, Nan::New("IPC_PRIVATE").ToLocalChecked(), Nan::New<Number>(IPC_PRIVATE));
		Nan::Set(target, Nan::New("IPC_CREAT").ToLocalChecked(), Nan::New<Number>(IPC_CREAT));
		Nan::Set(target, Nan::New("IPC_EXCL").ToLocalChecked(), Nan::New<Number>(IPC_EXCL));
		
		Nan::Set(target, Nan::New("SHM_RDONLY").ToLocalChecked(), Nan::New<Number>(SHM_RDONLY));
		
		Nan::Set(target, Nan::New("NODE_BUFFER_MAX_LENGTH").ToLocalChecked(), Nan::New<Number>(node::Buffer::kMaxLength));
		
		Nan::Set(target, Nan::New("O_CREAT").ToLocalChecked(), Nan::New<Number>(O_CREAT));
		Nan::Set(target, Nan::New("O_RDWR").ToLocalChecked(), Nan::New<Number>(O_RDWR));
		Nan::Set(target, Nan::New("O_RDONLY").ToLocalChecked(), Nan::New<Number>(O_RDONLY));
		Nan::Set(target, Nan::New("O_EXCL").ToLocalChecked(), Nan::New<Number>(O_EXCL));
		Nan::Set(target, Nan::New("O_TRUNC").ToLocalChecked(), Nan::New<Number>(O_TRUNC));
		
		Nan::Set(target, Nan::New("MAP_SHARED").ToLocalChecked(), Nan::New<Number>(MAP_SHARED));
		Nan::Set(target, Nan::New("MAP_PRIVATE").ToLocalChecked(), Nan::New<Number>(MAP_PRIVATE));

		Nan::Set(target, Nan::New("MAP_ANON").ToLocalChecked(), Nan::New<Number>(MAP_ANON));
		Nan::Set(target, Nan::New("MAP_ANONYMOUS").ToLocalChecked(), Nan::New<Number>(MAP_ANONYMOUS));
		Nan::Set(target, Nan::New("MAP_NORESERVE").ToLocalChecked(), Nan::New<Number>(MAP_NORESERVE));
		//Nan::Set(target, Nan::New("MAP_32BIT").ToLocalChecked(), Nan::New<Number>(MAP_32BIT));
		// Nan::Set(target, Nan::New("MAP_DENYWRITE").ToLocalChecked(), Nan::New<Number>(MAP_DENYWRITE));
		// Nan::Set(target, Nan::New("MAP_GROWSDOWN").ToLocalChecked(), Nan::New<Number>(MAP_GROWSDOWN));
		// Nan::Set(target, Nan::New("MAP_HUGETLB").ToLocalChecked(), Nan::New<Number>(MAP_HUGETLB));
		// Nan::Set(target, Nan::New("MAP_HUGE_2MB").ToLocalChecked(), Nan::New<Number>(MAP_HUGE_2MB));
		// Nan::Set(target, Nan::New("MAP_HUGE_1GB").ToLocalChecked(), Nan::New<Number>(MAP_HUGE_1GB));
		// Nan::Set(target, Nan::New("MAP_LOCKED").ToLocalChecked(), Nan::New<Number>(MAP_LOCKED));
		// Nan::Set(target, Nan::New("MAP_NONBLOCK").ToLocalChecked(), Nan::New<Number>(MAP_NONBLOCK));
		// Nan::Set(target, Nan::New("MAP_POPULATE").ToLocalChecked(), Nan::New<Number>(MAP_POPULATE));
		// Nan::Set(target, Nan::New("MAP_STACK").ToLocalChecked(), Nan::New<Number>(MAP_STACK));
		// Nan::Set(target, Nan::New("MAP_SYNC").ToLocalChecked(), Nan::New<Number>(MAP_SYNC));
		// Nan::Set(target, Nan::New("MAP_UNINITIALIZED").ToLocalChecked(), Nan::New<Number>(MAP_UNINITIALIZED));

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

		#if NODE_MODULE_VERSION < NODE_16_0_MODULE_VERSION
		node::AtExit(AtNodeExit);
		#else
		node::AddEnvironmentCleanupHook(target->GetIsolate(), AtNodeExit, nullptr);
		#endif
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
