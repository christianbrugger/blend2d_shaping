cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

cmake_policy(PUSH)

if(POLICY CMP0092)
    # Don't add -W3 warning level by default.
    cmake_policy(SET CMP0092 NEW)
endif()

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if (NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "blend2d_shaping")
    project(blend2d_shaping C CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Find Blend2d
if (NOT DEFINED BLEND2D_DIR)
    foreach(dir "${CMAKE_CURRENT_LIST_DIR}/../blend2d")
        if (EXISTS ${dir}/CMakeLists.txt)
            set(BLEND2D_DIR "${dir}" CACHE PATH "Location of 'blend2d'")
            break()
        endif()
    endforeach()
    if (NOT DEFINED BLEND2D_DIR)
        message(FATAL "Unable to find blend2d, please visit <https://blend2d.com/doc/build-instructions.html>")
    endif()
endif()
set(BLEND2D_STATIC TRUE)
add_subdirectory("${BLEND2D_DIR}" "${CMAKE_BINARY_DIR}/external/blend2d" EXCLUDE_FROM_ALL SYSTEM)

# Find harfbuzz
if (NOT DEFINED HB_DIR)
    foreach(dir "${CMAKE_CURRENT_LIST_DIR}/../harfbuzz")
        if (EXISTS ${dir}/CMakeLists.txt)
            set(HB_DIR "${dir}" CACHE PATH "Location of 'harfbuzz'")
            break()
        endif()
    endforeach()
    if (NOT DEFINED HB_DIR)
        message(FATAL "Unable to find harfbuzz")
    endif()
endif()
add_subdirectory("${HB_DIR}" "${CMAKE_BINARY_DIR}/external/harfbuzz" EXCLUDE_FROM_ALL SYSTEM)




# Library blend2d_shaping
add_library(blend2d_shaping STATIC
	"src/blend2d_shaping.cpp"
)
target_include_directories(blend2d_shaping PUBLIC
	src/
)
target_link_libraries(blend2d_shaping PUBLIC 
    Blend2D::Blend2D
)
target_link_libraries(blend2d_shaping PRIVATE 
    harfbuzz
)
target_compile_features(blend2d_shaping PUBLIC cxx_std_20)



# Example
set(MY_RESOURCE_FILE "fonts/NotoSans-Regular.ttf")
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE}
	COMMAND ${CMAKE_COMMAND} -E copy
		${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE}
		${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE}
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE}
)
add_executable(blend2d_shaping_example
	example/main.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE}
)
target_link_libraries(blend2d_shaping_example
	blend2d_shaping
)



        
cmake_policy(POP)
