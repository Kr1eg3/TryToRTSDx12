# ThirdParty dependencies

# Assimp configuration
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable Assimp tests")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable Assimp tools")
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Disable Assimp samples")
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp install")
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Disable debug postfix")
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build bundled zlib")

# Only build the formats we need to reduce compile time
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all importers by default")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "Enable OBJ importer")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "Enable FBX importer")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "Enable glTF importer")
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "Enable 3DS importer")

# Disable unnecessary exporters
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all exporters")

add_subdirectory(assimp)

# Set properties for better organization in IDE
set_target_properties(assimp PROPERTIES FOLDER "ThirdParty")
if(TARGET zlib)
    set_target_properties(zlib PROPERTIES FOLDER "ThirdParty")
endif()
if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "ThirdParty")
endif()
if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER "ThirdParty")
endif()