#==--- cmake/ConkyConfig.cmake ----------------------------------------------==#
#
#                                 	 Voxel 
#
#                         Copyright (c) 2017 Rob Clucas
#  
#  This file is distributed under the MIT License. See LICENSE for details.
#
#==--------------------------------------------------------------------------==#
#
# Description : This file defines the cmake configuration file for voxel.
#           
#==--------------------------------------------------------------------------==#

get_filename_component(Conky_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)

# Define the include directories:
set(Conky_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../include" )
set(Conky_LIBRARY_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../lib"     )
set(Conky_LIBRARYS     																	  					)
set(Conky_DEFINITIONS  "${Conky_DEFINITIONS}" 				  						)

set(SupportedComponents )

set(Conky_FOUND True)

# Check that all the components are found:
# And add the components to the Voxel_LIBS parameter:
foreach(comp ${Conky_FIND_COMPONENTS})
	if (NOT ";${SupportedComponents};" MATCHES comp)
		set(Conky_FOUND False)
		set(Conky_NOT_FOUND_MESSAGE "Unsupported component: ${comp}")
	endif()
	set(Conky_LIBS "${Conky_LIBS} -l{comp}")
	if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${comp}Targets.cmake")
		include("${CMAKE_CURRENT_LIST_DIR}/${comp}Targets.cmake")
	endif()
endforeach()