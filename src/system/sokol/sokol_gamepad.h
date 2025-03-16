#ifndef SOKOL_GAMEPAD_INCLUDED
/*
    sokol_gamepad.h -- cross-platform gamepad API

    Project URL: https://github.com/floooh/sokol

    Do this:
        #define SOKOL_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    To use:  
    - In you app initialization code call sgamepad_init()
    - At the exact time you want to record input state call sgamepad_record_state()
    - Get the state for a particular gamepad like this:
        sgamepad_gamepad_state state;
        sgamepad_get_gamepad_state(0, &state);
    
    sgamepad_gamepad_state's members are set to the contoller's state as recorded previously.

    Analog stick states are pre-processed to take dead zones into account: in most cases you should rely on
    direction_x/direction_y/magnitude for input processing.
*/

#define SOKOL_GAMEPAD_INCLUDED (1)
#include <stdint.h>
#include <stdbool.h>

#ifndef SOKOL_API_DECL
#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_IMPL)
#define SOKOL_API_DECL __declspec(dllexport)
#elif defined(_WIN32) && defined(SOKOL_DLL)
#define SOKOL_API_DECL __declspec(dllimport)
#else
#define SOKOL_API_DECL extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* Bit flags for digital_inputs bitfield */
    typedef enum sgamepad_digital_inputs {
        SGAMEPAD_GAMEPAD_DPAD_UP     = 0x0001,
        SGAMEPAD_GAMEPAD_DPAD_DOWN   = 0x0002,
        SGAMEPAD_GAMEPAD_DPAD_LEFT   = 0x0004,
        SGAMEPAD_GAMEPAD_DPAD_RIGHT  = 0x0008,
        SGAMEPAD_GAMEPAD_START       = 0x0010,
        SGAMEPAD_GAMEPAD_BACK        = 0x0020, // Select on DualShock
        SGAMEPAD_GAMEPAD_A           = 0x0040, // X on DualShock
        SGAMEPAD_GAMEPAD_B           = 0x0080, // Circle on DualShock
        SGAMEPAD_GAMEPAD_X           = 0x0100, // Square on DualShock
        SGAMEPAD_GAMEPAD_Y           = 0x0200, // Triangle on DualShock
        SGAMEPAD_GAMEPAD_LEFT_THUMB  = 0x0400, // L3 on DualShock
        SGAMEPAD_GAMEPAD_RIGHT_THUMB = 0x0800, // R3 on DualShock
    } sgamepad_digital_inputs;

    typedef struct sgamepad_analog_stick_state {
        float normalized_x; //X component as reported by underlying API, scaled 0 to 1
        float normalized_y; //Y component as reported by underlying API, scaled 0 to 1
        float direction_x; //X component of normalized direction vector
        float direction_y; //Y component of normalized direction vector
        float magnitude;   //Normalized magnitude
    } sgamepad_analog_stick_state;

    typedef struct sgamepad_gamepad_state {
        uint16_t digital_inputs;
        sgamepad_analog_stick_state left_stick;
        sgamepad_analog_stick_state right_stick;
        float left_shoulder;
        float right_shoulder;
        float left_trigger;
        float right_trigger;
    } sgamepad_gamepad_state;

    SOKOL_API_DECL unsigned int sgamepad_get_max_supported_gamepads();

    SOKOL_API_DECL void sgamepad_init();

    SOKOL_API_DECL void sgamepad_record_state();

    SOKOL_API_DECL void sgamepad_get_gamepad_state(unsigned int index, sgamepad_gamepad_state* pstate);

#ifdef __cplusplus
} /* extern "C" */

/* reference-based equivalents for c++ */

#endif
#endif // SOKOL_GAMEPAD_INCLUDED

#ifdef SOKOL_IMPL
#define SOKOL_GAMEPAD_IMPL_INCLUDED (1)
#include <string.h> /* memset */
#include <math.h>

#ifndef SOKOL_API_IMPL
    #define SOKOL_API_IMPL
#endif
#ifndef SOKOL_DEBUG
    #ifndef NDEBUG
        #define SOKOL_DEBUG (1)
    #endif
#endif
#ifndef SOKOL_ASSERT
    #include <assert.h>
    #define SOKOL_ASSERT(c) assert(c)
#endif
#ifndef SOKOL_UNREACHABLE
    #define SOKOL_UNREACHABLE SOKOL_ASSERT(false)
#endif
#if !defined(SOKOL_CALLOC) || !defined(SOKOL_FREE)
    #include <stdlib.h>
#endif
#if !defined(SOKOL_CALLOC)
    #define SOKOL_CALLOC(n,s) calloc(n,s)
#endif
#if !defined(SOKOL_FREE)
    #define SOKOL_FREE(p) free(p)
#endif
#ifndef SOKOL_LOG
    #ifdef SOKOL_DEBUG
        #if defined(__ANDROID__)
            #include <android/log.h>
            #define SOKOL_LOG(s) { SOKOL_ASSERT(s); __android_log_write(ANDROID_LOG_INFO, "SOKOL_APP", s); }
        #else
            #include <stdio.h>
            #define SOKOL_LOG(s) { SOKOL_ASSERT(s); puts(s); }
        #endif
    #else
        #define SOKOL_LOG(s)
    #endif
#endif
#ifndef SOKOL_ABORT
    #include <stdlib.h>
    #define SOKOL_ABORT() abort()
#endif
#ifndef _SOKOL_PRIVATE
    #if defined(__GNUC__) || defined(__clang__)
        #define _SOKOL_PRIVATE __attribute__((unused)) static
    #else
        #define _SOKOL_PRIVATE static
    #endif
#endif
#ifndef _SOKOL_UNUSED
    #define _SOKOL_UNUSED(x) (void)(x)
#endif

/*== COMMON INCLUDES AND DEFINES ==================================*/

_SOKOL_PRIVATE float _sgamepad_normalize_analog_trigger(float value, float max_value, float activation_value) {
    if (value < activation_value) {
        return 0.0f;
    }

    float output = (value - activation_value) / (max_value - activation_value);
    return output;
}

_SOKOL_PRIVATE void _sgamepad_generate_analog_stick_state(float x_value, float y_value, float max_magnitude, float dead_zone_magnitude, sgamepad_analog_stick_state* pstate) {
    float magnitude = 0.0f;
    if (max_magnitude != 1.0f) {
        pstate->normalized_x = fmax(fmin(x_value/max_magnitude, 1.0f), -1.0f);
        pstate->normalized_y = fmax(fmin(y_value/max_magnitude, 1.0f), -1.0f);
        magnitude = sqrtf(x_value * x_value + y_value * y_value);
    } else {
        pstate->normalized_x = x_value;
        pstate->normalized_y = y_value;
        magnitude = fmin(sqrtf(x_value * x_value + y_value * y_value), 1.0f);
    }
        
    if (magnitude <= dead_zone_magnitude) {
        pstate->direction_x = 0.0f;
        pstate->direction_y = 0.0f;
        pstate->magnitude = 0.0f;
        return;
    }

    pstate->direction_x = x_value / magnitude;
    pstate->direction_y = y_value / magnitude;
    magnitude = fmin(magnitude, max_magnitude);
    pstate->magnitude = (magnitude - dead_zone_magnitude) / (max_magnitude - dead_zone_magnitude);
}

/*== PLATFORM SPECIFIC INCLUDES AND DEFINES ==================================*/
#if defined (_SAPP_WIN32) || defined(_SAPP_APPLE) || defined(_SAPP_LINUX)
    #include <GLFW/glfw3.h>
#else
    #define SGAMEPAD_MAX_SUPPORTED_GAMEPADS 0
#endif

#define SGAMEPAD_MAX_SUPPORTED_GAMEPADS 4

typedef struct sgamepad {
    sgamepad_gamepad_state gamepad_states[SGAMEPAD_MAX_SUPPORTED_GAMEPADS];
} sgamepad;

_SOKOL_PRIVATE sgamepad _sgamepad = {0};

/*== Desktop Implementation ============================================*/
#if defined (_SAPP_WIN32) || defined(_SAPP_APPLE) || defined(_SAPP_LINUX)

_SOKOL_PRIVATE void _sgamepad_record_state() {
    for (int i = 0; i < SGAMEPAD_MAX_SUPPORTED_GAMEPADS; i++)
    {
        sgamepad_gamepad_state* target = _sgamepad.gamepad_states + i;

        uint16_t new_flags = 0;

        GLFWgamepadstate state;
        if(glfwGetGamepadState(i, &state))
        {
            if(state.buttons[GLFW_GAMEPAD_BUTTON_A])            new_flags |= SGAMEPAD_GAMEPAD_A;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_B])            new_flags |= SGAMEPAD_GAMEPAD_B;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_X])            new_flags |= SGAMEPAD_GAMEPAD_X;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_Y])            new_flags |= SGAMEPAD_GAMEPAD_Y;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_BACK])         new_flags |= SGAMEPAD_GAMEPAD_BACK;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_START])        new_flags |= SGAMEPAD_GAMEPAD_START;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB])   new_flags |= SGAMEPAD_GAMEPAD_LEFT_THUMB;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB])  new_flags |= SGAMEPAD_GAMEPAD_RIGHT_THUMB;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP])      new_flags |= SGAMEPAD_GAMEPAD_DPAD_UP;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT])   new_flags |= SGAMEPAD_GAMEPAD_DPAD_RIGHT;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN])    new_flags |= SGAMEPAD_GAMEPAD_DPAD_DOWN;
            if(state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT])    new_flags |= SGAMEPAD_GAMEPAD_DPAD_LEFT;

        }

        target->digital_inputs = new_flags;

        _sgamepad_generate_analog_stick_state(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 1.0f, 0.01f, &(target->left_stick));
        _sgamepad_generate_analog_stick_state(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 1.0f, 0.01f, &(target->right_stick));

        target->left_shoulder = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] ? 1.0f : 0.0f;
        target->right_shoulder = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] ? 1.0f : 0.0f;
        target->left_trigger = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
        target->right_trigger = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    }
}

/*== Fallback dummy Implementation ============================================*/
#else

_SOKOL_PRIVATE void _sgamepad_record_state() {
}

#endif

/*=== PUBLIC API FUNCTIONS ===================================================*/
SOKOL_API_IMPL unsigned int sgamepad_get_max_supported_gamepads() {
    return SGAMEPAD_MAX_SUPPORTED_GAMEPADS;
}

SOKOL_API_IMPL void sgamepad_init() {

#if defined (_SAPP_WIN32) || defined(_SAPP_APPLE) || defined(_SAPP_LINUX)
    glfwInit();
#endif
}

SOKOL_API_IMPL void sgamepad_record_state() {
    memset(_sgamepad.gamepad_states, 0, sizeof(_sgamepad.gamepad_states));
    _sgamepad_record_state();
}

SOKOL_API_IMPL void sgamepad_get_gamepad_state(unsigned int index, sgamepad_gamepad_state* pstate) {
    if (index < SGAMEPAD_MAX_SUPPORTED_GAMEPADS) {
        *pstate = _sgamepad.gamepad_states[index];
    }
}

#endif /* SOKOL_IMPL */
