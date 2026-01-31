cmake_minimum_required(VERSION 4.1)
project(Minecraft)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Forza l'architettura Apple Silicon (M1/M2/M3/M4)
set(CMAKE_OSX_ARCHITECTURES "arm64")

# --- DIPENDENZE ESTERNE (VENDORED) ---

# 1. GLFW - Lo compiliamo dai sorgenti in external/glfw
# Disabilitiamo la fuffa che non ci serve per velocizzare il build
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/CMakeLists.txt")
    add_subdirectory(external/glfw)
else()
    message(FATAL_ERROR "OH! Ti sei dimenticato di mettere GLFW in external/glfw. Scaricalo!")
endif()

# 2. GLM - È header-only, basta dire dove sono i file
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm/glm")
    include_directories(external/glm)
else()
    message(FATAL_ERROR "GLM non trovato in external/glm. Sistema sta cartella.")
endif()

# --- CONFIGURAZIONE SORGENTI ---

# Includi la tua cartella header
include_directories(include)

# Cerca tutti i file .cpp nella cartella src
set(SOURCES
        src/main.cpp
        src/Shader.cpp
)
# --- TARGET FINALE ---

add_executable(${PROJECT_NAME} ${SOURCES})

# Linkiamo tutto: la libreria 'glfw' (prodotta dal subdirectory) e i framework Apple
target_link_libraries(${PROJECT_NAME}
        glfw
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework QuartzCore"
)

# Messaggino di flex per ricordarti che sei su un M4
message(STATUS "Configurazione completata per ${CMAKE_OSX_ARCHITECTURES}. Al lavoro, alfanowski.")