#==--- docs/CMakeLists.txt --------------------------------------------------==#

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in 
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile 
    @ONLY)

  if (NOT EXISTS DoxyFormat)
  	configure_file(
  		${CMAKE_CURRENT_SOURCE_DIR}/FetchFiles.py
  		${CMAKE_CURRENT_BINARY_DIR}/FetchFiles.py
  		@ONLY)

 			add_custom_command(OUTPUT 					 DoxyFormat
					  						 COMMAND 					 python FetchFiles.py
						    				 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
 	endif()
 	
  add_custom_target(Docs
    COMMAND 					${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    DEPENDS 					DoxyFormat
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} 
    COMMENT 					"Generate doxygen documentation")
endif(DOXYGEN_FOUND)