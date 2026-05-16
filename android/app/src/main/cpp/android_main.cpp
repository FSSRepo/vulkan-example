#include "vk_android_example.h"
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/log.h>

#define TAG "Vulkan-Example"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

Engine* g_currentEngine = nullptr;

static void initVulkan(Engine* engine) {
    if (!loadVulkanLibrary()) {
        LOGE("Vulkan is unavailable");
        return;
    }
    engine->inst = VulkanInstance(false);
    engine->inst.attach(engine->app->window);
    engine->inst.initializeDevice();

    engine->width = ANativeWindow_getWidth(engine->app->window);
    engine->height = ANativeWindow_getHeight(engine->app->window);

    engine->chain = new VulkanSwapchain(engine->inst);
    engine->chain->initalize(engine->width, engine->height, true);

    engine->renderer = new VulkanRenderer(*(engine->chain));
    engine->renderer->initialize(engine->inst);

    engine->menuExample = createMenuExample();
    engine->currentExample = engine->menuExample;
    engine->currentExample->init(engine->app, engine->inst, *(engine->chain), engine->width, engine->height);

    g_currentEngine = engine;
    engine->initialized = true;
}

static void cleanupVulkan(Engine* engine) {
    if (!engine->initialized) return;
    vkDeviceWaitIdle(engine->inst.device);
    if (engine->currentExample) {
        engine->currentExample->cleanup(engine->inst);
        if (engine->currentExample != engine->menuExample) {
            delete engine->currentExample;
        }
        engine->currentExample = nullptr;
    }
    if (engine->menuExample) {
        delete engine->menuExample;
        engine->menuExample = nullptr;
    }
    engine->renderer->destroy();
    delete engine->renderer;
    engine->chain->destroy();
    delete engine->chain;
    engine->inst.destroy();
    engine->initialized = false;
}

void androidSetExample(Engine* engine, IExample* example) {
    if (!engine->initialized) return;
    vkDeviceWaitIdle(engine->inst.device);
    if (engine->currentExample && engine->currentExample != engine->menuExample) {
        engine->currentExample->cleanup(engine->inst);
        delete engine->currentExample;
    } else if (engine->currentExample == engine->menuExample) {
        engine->currentExample->cleanup(engine->inst);
    }
    engine->currentExample = example;
    example->init(engine->app, engine->inst, *(engine->chain), engine->width, engine->height);
}

void androidReturnToMenu(Engine* engine) {
    if (engine->currentExample && engine->currentExample != engine->menuExample) {
        androidSetExample(engine, engine->menuExample);
    }
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        bool down = (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE);
        if (engine->currentExample) {
            engine->currentExample->onTouch(x, y, action, down);
        }
        return 1;
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        int32_t keyAction = AKeyEvent_getAction(event);
        if (keyCode == AKEYCODE_BACK && keyAction == AKEY_EVENT_ACTION_UP) {
            androidReturnToMenu(engine);
            return 1;
        }
    }
    return 0;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    Engine* engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr && !engine->initialized) {
                initVulkan(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            cleanupVulkan(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->animating = false;
            break;
    }
}

void android_main(struct android_app* state) {
    Engine engine = {};
    engine.app = state;
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    state->onInputEvent = handleInput;

    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                cleanupVulkan(&engine);
                return;
            }
        }
        if (engine.animating && engine.initialized && engine.currentExample) {
            engine.currentExample->draw(*(engine.renderer), engine.renderer->currentFrame);
        }
    }
}
