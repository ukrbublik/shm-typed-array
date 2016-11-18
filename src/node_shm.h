#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "nan.h"
#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

using namespace node;
using namespace v8;

/*
namespace imp {
	static const size_t kMaxLength = 0x3fffffff;
}

namespace node {
namespace Buffer {
	// 2GB (0x7fffffff) for 64bit, 1GB for 32bit
	static const unsigned int kMaxLength = 
		sizeof(int32_t) == sizeof(intptr_t) ? 0x3fffffff : 0x7fffffff;
}
}
*/

#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#define SAFE_DELETE_ARR(a) if( (a) != NULL ) delete [] (a); (a) = NULL;


enum ShmBufferType {
	SHMBT_BUFFER = 0, //for using Buffer instead of TypedArray
	SHMBT_INT8,
	SHMBT_UINT8,
	SHMBT_UINT8CLAMPED,
	SHMBT_INT16,
	SHMBT_UINT16,
	SHMBT_INT32,
	SHMBT_UINT32,
	SHMBT_FLOAT32,
	SHMBT_FLOAT64
};


namespace node {
namespace Buffer {

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t length
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
		, ShmBufferType type = SHMBT_FLOAT64
	);

}
}


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
		, ShmBufferType type = SHMBT_FLOAT64
	);

}


namespace node {
namespace node_shm {

	/**
	 * Create or get shared memory
	 * Params:
	 *  key_t key
	 *  size_t size
	 *  int shmflg - flags for shmget()
	 *  int at_shmflg - flags for shmat()
	 *  enum ShmBufferType type
	 * Returns buffer or typed array, depends on input param type
	 */
	NAN_METHOD(get);

	/**
	 * Destroy shared memory segment
	 * Params:
	 *  key_t key
	 *  bool force - true to destroy even there are other processed uses this segment
	 * Returns count of left attaches or -1 on error
	 */
	NAN_METHOD(detach);

	/**
	 * Detach all created and getted shared memory segments
	 * Returns count of destroyed segments
	 */
	NAN_METHOD(detachAll);

	/**
	 * Get total size of all shared segments in bytes
	 */
	NAN_METHOD(getTotalSize);

	/**
	 * Constants to be exported:
	 * IPC_PRIVATE
	 * IPC_CREAT
	 * IPC_EXCL
	 * SHM_RDONLY
	 * NODE_BUFFER_MAX_LENGTH
	 * enum ShmBufferType: 
	 *  SHMBT_BUFFER, SHMBT_INT8, SHMBT_UINT8, SHMBT_UINT8CLAMPED, 
	 *  SHMBT_INT16, SHMBT_UINT16, SHMBT_INT32, SHMBT_UINT32, 
	 *  SHMBT_FLOAT32, SHMBT_FLOAT64
	 */

}
}
