cmake_minimum_required(VERSION 3.22)
project(AudioMasterPro VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# JUCE fixado em uma tag para builds reproduzíveis
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)

# Catch2 (framework de testes)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    ON)

FetchContent_MakeAvailable(JUCE Catch2)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------
# AMP_Core: nucleo (DSP, metering, engine, presets). SEM JUCE e sem UI.
# E o mesmo nucleo que o Standalone usa e que o wrapper VST3 (Fase 2) vai usar.
# ---------------------------------------------------------------
add_library(AMP_Core STATIC
    Source/Audio/AudioEngine.cpp
    Source/Metering/LoudnessMeter.cpp
    Source/Presets/Presets.cpp
    Source/Utilities/SystemInfo.cpp)
target_include_directories(AMP_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_features(AMP_Core PUBLIC cxx_std_20)
target_link_libraries(AMP_Core PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(AMP_Core PRIVATE /W4)
endif()

# ---------------------------------------------------------------
# Aplicativo Standalone
# ---------------------------------------------------------------
juce_add_gui_app(AudioMasterPro
    PRODUCT_NAME "Audio Master Pro"
    COMPANY_NAME "Audio Master Pro"
    VERSION ${PROJECT_VERSION})

target_sources(AudioMasterPro PRIVATE
    Source/Application/Main.cpp
    Source/Application/AppController.h
    Source/UI/MainComponent.cpp
    Source/UI/MainComponent.h
    Source/UI/Theme.h
    Source/UI/Widgets.h
    Source/UI/MixerPanel.h
    Source/UI/EqPanel.h
    Source/UI/DynamicsPanel.h
    Source/UI/LoudnessPanel.h
    Source/UI/AudioSettingsPanel.h)

target_compile_definitions(AudioMasterPro PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="Audio Master Pro"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

# ASIO (opcional): baixe o ASIO SDK da Steinberg (a licenca nao permite redistribui-lo no repositorio)
# e configure com:  cmake --preset windows-release -DAMP_ASIO_SDK_DIR="C:/caminho/ASIOSDK"
set(AMP_ASIO_SDK_DIR "" CACHE PATH "Pasta do ASIO SDK (opcional, habilita o driver ASIO)")
if(AMP_ASIO_SDK_DIR)
    target_compile_definitions(AudioMasterPro PRIVATE JUCE_ASIO=1)
    target_include_directories(AudioMasterPro PRIVATE
        ${AMP_ASIO_SDK_DIR}/common ${AMP_ASIO_SDK_DIR}/host ${AMP_ASIO_SDK_DIR}/host/pc)
endif()

target_link_libraries(AudioMasterPro
    PRIVATE
        AMP_Core
        juce::juce_core
        juce::juce_events
        juce::juce_gui_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------
# Testes
# ---------------------------------------------------------------
enable_testing()
add_subdirectory(Tests)
