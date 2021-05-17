cmake_minimum_required(VERSION 3.11)  # 3.5 if FetchContent not used
project(ipclib)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
add_library(ipclib STATIC ${SOURCES})
target_include_directories(ipclib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(Threads REQUIRED)
find_package(Boost)
target_link_libraries(ipclib PRIVATE Threads::Threads)

if (NOT WIN32)
    target_link_libraries(ipclib PRIVATE rt)
endif()

if (Boost_FOUND)
	target_link_libraries(ipclib PRIVATE Boost::boost)
elseif(DOWNLOAD_BOOSTS)
	message(STATUS "Downloading boosts...")

    set(BOOSTS
        "assert"
        "bind"
        "config"
        "core"
        "date_time"
        "interprocess"
        "intrusive"
        "move"
        "mpl"
        "numeric_conversion"
        "predef"
        "preprocessor"
        "regex"
        "smart_ptr"
        "static_assert"
        "throw_exception"
        "type_traits"
        "utility"
    )
    
    include(FetchContent)
    foreach(BOOST ${BOOSTS})
        FetchContent_Declare(
            boost_${BOOST}
            GIT_REPOSITORY  https://github.com/boostorg/${BOOST}.git
        	GIT_SHALLOW     1
        )
        FetchContent_Populate(boost_${BOOST})
        target_include_directories(ipclib PRIVATE "${boost_${BOOST}_SOURCE_DIR}/include")
	endforeach()
else()
	message(FATAL_ERROR "Boost not found and DOWNLOAD_BOOSTS is false")
endif()
