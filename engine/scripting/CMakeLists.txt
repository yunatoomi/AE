cmake_minimum_required( VERSION 3.10 FATAL_ERROR )

if (${AE_ENABLE_ANGELSCRIPT})
	set( AE_ENABLE_SCRIPTING ON CACHE INTERNAL "" FORCE )
endif ()

if (${AE_ENABLE_SCRIPTING})
	file( GLOB_RECURSE SOURCES "*.*" )
	add_library( "Scripting" STATIC ${SOURCES} )
	source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} )
	set_property( TARGET "Scripting" PROPERTY FOLDER "Engine" )
	target_link_libraries( "Scripting" PUBLIC "STL" )
	target_compile_definitions( "Scripting" PUBLIC "AE_ENABLE_SCRIPTING" )
	
	if (${AE_ENABLE_ANGELSCRIPT})
		target_link_libraries( "Scripting" PUBLIC "AngelScript-lib" ) # TODO: make private
	endif ()
endif()
