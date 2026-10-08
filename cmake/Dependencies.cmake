include(FetchContent)
if(CORESIM_BUILD_APP)
    if(CORESIM_FETCH_DEPENDENCIES)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        endif()
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # 3.4
            SYSTEM)
        FetchContent_MakeAvailable(glfw)

    else()
        find_package(glfw3 3.4 CONFIG REQUIRED)
    endif()
endif()
if(BUILD_TESTING)
    if(CORESIM_FETCH_DEPENDENCIES)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # 3.7.1
            SYSTEM)
        FetchContent_MakeAvailable(Catch2)
    else()
        find_package(Catch2 3 CONFIG REQUIRED)
    endif()
endif()

# Scene math is also required by headless camera/transform tests.
if(CORESIM_FETCH_DEPENDENCIES)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863 # 1.0.1
        SYSTEM)
    FetchContent_MakeAvailable(glm)
else()
    find_package(glm 1.0 CONFIG REQUIRED)
endif()
