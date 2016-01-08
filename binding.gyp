{
	"targets": [{
		"target_name": "shm",
		"include_dirs": [
			"src",
			"<!(node -e \"require('nan')\")",
		],
		"sources": [
			"src/shm.cc",
		]
	}]
}
