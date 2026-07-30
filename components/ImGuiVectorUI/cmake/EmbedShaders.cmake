if(NOT DEFINED IVUI_SHADER_DIRECTORY OR NOT DEFINED IVUI_OUTPUT OR
   NOT DEFINED IVUI_FUNCTION_NAME)
    message(FATAL_ERROR "Shader directory, output and function name are required.")
endif()

file(GLOB shader_files LIST_DIRECTORIES false "${IVUI_SHADER_DIRECTORY}/*")
list(SORT shader_files)
if(NOT shader_files)
    message(FATAL_ERROR "No compiled shaders found in ${IVUI_SHADER_DIRECTORY}.")
endif()

set(content "#include \"ImGuiVectorUI/Backends/ShaderLibrary.h\"\n\n")
string(APPEND content "#include <cstdint>\n#include <cstring>\n\n")
string(APPEND content "namespace IVUI\n{\n    namespace\n    {\n")

set(lookup)
foreach(shader IN LISTS shader_files)
    get_filename_component(filename "${shader}" NAME)
    string(MAKE_C_IDENTIFIER "${filename}" identifier)
    file(READ "${shader}" shader_hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," shader_bytes "${shader_hex}")
    string(APPEND content
        "        alignas(std::uint32_t) const unsigned char ${identifier}[] = {${shader_bytes}};\n")
    string(APPEND content
        "        static_assert(sizeof(${identifier}) % sizeof(std::uint32_t) == 0);\n")
    string(APPEND lookup
        "        if (std::strcmp(filename, \"${filename}\") == 0)\n"
        "            return {reinterpret_cast<const std::uint32_t*>(${identifier}), sizeof(${identifier})};\n")
endforeach()

string(APPEND content "    }\n\n")
string(APPEND content
    "    ShaderBinary ShaderLibrary::${IVUI_FUNCTION_NAME}(const char* filename) noexcept\n"
    "    {\n"
    "        if (!filename)\n"
    "            return {};\n"
    "${lookup}"
    "        return {};\n"
    "    }\n"
    "}\n")

if(EXISTS "${IVUI_OUTPUT}")
    file(READ "${IVUI_OUTPUT}" existing_content)
    if(existing_content STREQUAL content)
        return()
    endif()
endif()
file(WRITE "${IVUI_OUTPUT}" "${content}")
