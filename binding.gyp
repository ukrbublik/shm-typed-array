{
	"targets": [{
		"target_name": "shm",
		"include_dirs": [
			"src",
			"<!(node -e \"require('nan')\")",
		],
		"sources": [
			"src/node_shm.h",
			"src/node_shm.cc"
		]
	}]
}
