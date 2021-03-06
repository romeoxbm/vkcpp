# TINYXML2_FOUND
# TINYXML2_INCLUDE_DIR
# TINYXML2_LIBRARIES

find_path( TINYXML2_INCLUDE_DIR
	NAMES tinyxml2.h
	PATHS
		/include
		/usr/include
		/usr/local/include
)

find_library( TINYXML2_LIBRARIES
	NAMES tinyxml tinyxml2
	PATHS
		/lib
		/lib/x86_64-linux-gnu
		/usr/lib
		/usr/lib/x86_64-linux-gnu
		/usr/local/lib
)

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( TINYXML2 DEFAULT_MSG TINYXML2_LIBRARIES TINYXML2_INCLUDE_DIR )

mark_as_advanced( TINYXML2_INCLUDE_DIR TINYXML2_LIBRARIES )
