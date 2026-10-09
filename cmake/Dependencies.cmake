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

# Dear ImGui and its official backends are needed only for the interactive application.
if(CORESIM_BUILD_APP)
    if(CORESIM_FETCH_DEPENDENCIES)
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06 # v1.91.9b
            SYSTEM)
        FetchContent_MakeAvailable(imgui)
        set(CORESIM_IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}")
    else()
        set(CORESIM_IMGUI_SOURCE_DIR "" CACHE PATH "Dear ImGui source directory (v1.91.9b, including backends)")
        if(NOT EXISTS "${CORESIM_IMGUI_SOURCE_DIR}/imgui.cpp")
            message(FATAL_ERROR "Set CORESIM_IMGUI_SOURCE_DIR to Dear ImGui v1.91.9b sources")
        endif()
    endif()
    add_library(coresim_imgui STATIC
        ${CORESIM_IMGUI_SOURCE_DIR}/imgui.cpp
        ${CORESIM_IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${CORESIM_IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${CORESIM_IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${CORESIM_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${CORESIM_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(coresim_imgui SYSTEM PUBLIC ${CORESIM_IMGUI_SOURCE_DIR})
    target_compile_definitions(coresim_imgui PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(coresim_imgui PRIVATE glfw ${CMAKE_DL_LIBS})
endif()
