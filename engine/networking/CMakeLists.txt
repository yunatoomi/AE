cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

if (${AE_ENABLE_CURL})
	set( AE_ENABLE_NETWORKING ON CACHE INTERNAL "" FORCE )
endif ()

if (${AE_ENABLE_NETWORKING})
	file( GLOB_RECURSE SOURCES "*.*" )
	add_library( "Networking" STATIC ${SOURCES} )
	source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
	set_property( TARGET "Networking" PROPERTY FOLDER "Engine" )
	target_link_libraries( "Networking" PUBLIC "Threading" )
	target_compile_definitions( "Networking" PUBLIC "AE_ENABLE_NETWORKING" )

	if (${AE_ENABLE_CURL})
		target_link_libraries( "Networking" PRIVATE "CURL-lib" )
	endif ()
endif ()
