cmake_minimum_required(VERSION 3.22)

# Configuración multi-plataforma Apple:
# - En iOS: arm64 nativo compatible desde iOS 12.0+ (incluye iPad mini 2 con Apple A7 / iOS 12.5.7 hasta iPadOS 18+)
# - En macOS: Universal Binary (Apple Silicon arm64 + Intel x86_64) desde macOS 10.15+
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build 64-bit arm64 iOS binary (A7 iPad mini 2+)" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS version (12.0 for iPad mini 2)" FORCE)
elseif(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal macOS binaries" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(XVBassPedal VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XV_IOS_INCLUDE_AUV3 "Include AUv3 App Extension in iOS build (GarageBand / AUM)" OFF)

# Busca primero JUCE preinstalado, luego ruta local (-DJUCE_DIR=...), o descarga JUCE 8.0.4 automáticamente
find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
    if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
        add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

# Formatos según plataforma:
# - iOS: App nativa Standalone (+ AUv3 opcional para GarageBand iOS / AUM)
# - macOS: VST3 + AU (.component Logic Pro) + Standalone
# - Windows / Linux: VST3 + Standalone
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLUGIN_FORMATS Standalone)
    if(XV_IOS_INCLUDE_AUV3)
        list(APPEND PLUGIN_FORMATS AUv3)
    endif()
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 Standalone AU)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(XVBassPedal
    COMPANY_NAME                  "XV Audio"
    COMPANY_WEBSITE               "https://github.com/Somospopups/Plugin-Bass"
    BUNDLE_ID                     "com.xvaudio.xvbasspedal"
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              FALSE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   TRUE
    COPY_PLUGIN_AFTER_BUILD       FALSE
    PLUGIN_MANUFACTURER_CODE      XvAu
    PLUGIN_CODE                   XvBp
    FORMATS                       ${PLUGIN_FORMATS}
    PRODUCT_NAME                  "XV Bass Pedal"
    ICON_BIG                      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app_icon_512.png"
    ICON_SMALL                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app_icon_180.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "XV Bass Pedal usa la entrada de audio para procesar tu bajo electrico en tiempo real (placa USB, iRig o microfono)."
    BACKGROUND_AUDIO_ENABLED      TRUE
    FILE_SHARING_ENABLED          TRUE
    DOCUMENT_BROWSER_ENABLED      FALSE
    STATUS_BAR_HIDDEN             TRUE
    REQUIRES_FULL_SCREEN          FALSE
    TARGETED_DEVICE_FAMILY        "1,2"
)

juce_generate_juce_header(XVBassPedal)

target_sources(XVBassPedal
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/XVBassDSP.h
        Source/DSP/XVCabIRs.h
        Source/DSP/XVBassTuner.h
        Source/DSP/XVPitchShifter.h
        Source/DSP/XVProfileExporter.h
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Permite compilar el .app / .ipa en GitHub Actions o Xcode sin certificado pago ($99/año),
    # listo para firmar gratis en 30 segundos con Sideloadly (Windows/Mac) en iOS 12.5.7+
    foreach(ios_tgt XVBassPedal XVBassPedal_Standalone XVBassPedal_AUv3)
        if(TARGET ${ios_tgt})
            set_target_properties(${ios_tgt} PROPERTIES
                XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0"
                XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
            )
        endif()
    endforeach()
endif()

if(MINGW)
    set_source_files_properties(
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        PROPERTIES COMPILE_FLAGS "-O1"
    )
    target_link_options(XVBassPedal PUBLIC -static -static-libgcc -static-libstdc++ -Wl,--allow-multiple-definition -Wl,-s)
    if(EXISTS "/var/tmp/xv_build/gb_parts/gb_part2.cpp")
        target_sources(XVBassPedal PRIVATE
            /var/tmp/xv_build/gb_parts/gb_part2.cpp
            /var/tmp/xv_build/gb_parts/gb_part3.cpp
            /var/tmp/xv_build/gb_parts/gb_part4.cpp
        )
    endif()
endif()

# Empaquetado de todos los gráficos físicos del pedal dentro del binario (.app / .vst3 / .component)
juce_add_binary_data(XVBassPedalAssets
    NAMESPACE BinaryData
    SOURCES
        Resources/pedal_base_active.png
        Resources/pedal_base_bypass.png
        Resources/knob_pointer_filmstrip_128.png
        Resources/cab_switch_0_bass.png
        Resources/cab_switch_1_punk.png
        Resources/cab_switch_2_clean.png
        Resources/footswitch_0_up.png
        Resources/footswitch_1_down.png
)

target_compile_definitions(XVBassPedal
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(NOT MSVC AND NOT MINGW)
    set_source_files_properties(Source/PluginProcessor.cpp PROPERTIES COMPILE_OPTIONS "-O3")
endif()

target_link_libraries(XVBassPedal
    PRIVATE
        XVBassPedalAssets
        juce::juce_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
)

if(APPLE)
    target_link_libraries(XVBassPedal PRIVATE "-framework Accelerate")
endif()

if(NOT MINGW)
    target_link_libraries(XVBassPedal
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endif()
