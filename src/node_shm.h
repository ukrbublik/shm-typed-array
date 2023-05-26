#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "nan.h"
#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <vector>
#include <string>

using namespace node;
using namespace v8;


/*
namespace imp {
	static const size_t kMaxLength = 0x3fffffff;
}

namespace node {
namespace Buffer {
	// 2^31 for 64bit, 2^30 for 32bit
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

inline int getSizeForShmBufferType(ShmBufferType type) {
	size_t size1 = 0;
	switch(type) {
		case SHMBT_BUFFER:
		case SHMBT_INT8:
		case SHMBT_UINT8:
		case SHMBT_UINT8CLAMPED:
			size1 = 1;
		break;
		case SHMBT_INT16:
		case SHMBT_UINT16:
			size1 = 2;
		break;
		case SHMBT_INT32:
		case SHMBT_UINT32:
		case SHMBT_FLOAT32:
			size1 = 4;
		break;
		default:
		case SHMBT_FLOAT64:
			size1 = 8;
		break;
	}
	return size1;
}


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
	 * Create or get System V shared memory segment
	 * Params:
	 *  key_t key
	 *  size_t count - count of elements, not bytes
	 *  int shmflg - flags for shmget()
	 *  int at_shmflg - flags for shmat()
	 *  enum ShmBufferType type
	 * Returns buffer or typed array, depends on input param type
	 * If not exists/alreeady exists, returns null
	 */
	NAN_METHOD(get);

	/**
	 * Create or get POSIX shared memory object
	 * Params:
	 *  String name
	 *  size_t count - count of elements, not bytes
	 *  int oflag - flag for shm_open()
	 *  mode_t mode - mode for shm_open()
	 *  int mmap_flags - flags for mmap()
	 *  enum ShmBufferType type
	 * Returns buffer or typed array, depends on input param type
	 * If not exists/alreeady exists, returns null
	 */
	NAN_METHOD(getPosix);

	/**
	 * Detach System V shared memory segment
	 * Params:
	 *  key_t key
	 *  bool force - true to destroy even there are other processed uses this segment
	 * Returns 0 if deleted, or count of left attaches, or -1 if not exists
	 */
	NAN_METHOD(detach);

	/**
	 * Detach POSIX shared memory object
	 * Params:
	 *  String name
	 *  bool force - true to destroy
	 * Returns 0 if deleted, 1 if detached, -1 if not exists
	 */
	NAN_METHOD(detachPosix);

	/**
	 * Detach all created and getted shared memory segments and objects
	 * Returns count of destroyed System V segments
	 */
	NAN_METHOD(detachAll);

	/**
	 * Get total size of all *created* shared memory in bytes
	 */
	NAN_METHOD(getTotalAllocatedSize);

	/**
	 * Get total size of all *used* shared memory in bytes
	 */
	NAN_METHOD(getTotalUsedSize);

	/**
	 * Constants to be exported:
	 * IPC_PRIVATE, IPC_CREAT, IPC_EXCL
	 * SHM_RDONLY
	 * NODE_BUFFER_MAX_LENGTH (count of elements, not bytes)
	 * O_CREAT, O_RDWR, O_RDONLY, O_EXCL, O_TRUNC
	 * enum ShmBufferType: 
	 *  SHMBT_BUFFER, SHMBT_INT8, SHMBT_UINT8, SHMBT_UINT8CLAMPED, 
	 *  SHMBT_INT16, SHMBT_UINT16, SHMBT_INT32, SHMBT_UINT32, 
	 *  SHMBT_FLOAT32, SHMBT_FLOAT64
	 */

}
}
