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


namespace node {
namespace Buffer {

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t length
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
	);

}


namespace node {
namespace node_shm {

	// Create or get shared memory
	// Params:
	//  key_t key
	//  size_t size
	//  int shmflg - flags for shmget()
	//  int at_shmflg - flags for shmat()
	//  todo typed, [][] ????
	NAN_METHOD(get);

	// Destroy shared memory segment
	// Params:
	//  key_t key
	NAN_METHOD(destroy);

	// Destroy all created shared memory segments
	NAN_METHOD(destroyAll);

	// Constants to be exported:
	// IPC_PRIVATE
	// IPC_CREAT
	// IPC_EXCL
	// SHM_RDONLY
	// NODE_BUFFER_MAX_LENGTH

}
}
