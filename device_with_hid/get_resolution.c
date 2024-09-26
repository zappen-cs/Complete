#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "debug_info.h"
#include "cJSON.h"

struct display_info {
    int width;
    int height;
};

// wl_output listener handlers (回调函数)
static void handle_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model, int32_t transform) {
    // 这里可以处理显示器几何信息，暂时不需要用到
}

static void handle_mode(void *data, struct wl_output *wl_output,
                        uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    struct display_info *info = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        info->width = width;
        info->height = height;
        printf("Found mode: %dx%d\n", width, height);  // 调试输出
    }
}

static void handle_done(void *data, struct wl_output *wl_output) {
    printf("Output done\n");  // 调试输出
}

static void handle_scale(void *data, struct wl_output *wl_output, int32_t factor) {
    // 如果需要处理缩放，可以在这里处理
}

// The wl_output listener (输出监听器)
static const struct wl_output_listener output_listener = {
    .geometry = handle_geometry,
    .mode = handle_mode,
    .done = handle_done,
    .scale = handle_scale
};

// wl_registry listener callback (注册回调函数)
static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface, uint32_t version) {
    struct display_info *info = data;
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        wl_output_add_listener(output, &output_listener, info);
        DEBUG_INFO("Bound wl_output to registry");  // 调试输出
    }
}

static void registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
    // 如果需要处理全局对象的移除，可以在这里处理
}

int get_resolution(int *width, int *height){
    struct wl_display *display = wl_display_connect(NULL);
    struct display_info info = {0, 0};
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);

    // wl_registry listener (注册监听器)
    static const struct wl_registry_listener registry_listener = {
        .global = registry_global_handler,
        .global_remove = registry_global_remove_handler
    };

    wl_registry_add_listener(registry, &registry_listener, &info);

    // 调用 wl_display_roundtrip() 以确保事件被处理
    wl_display_roundtrip(display);  // 获取注册对象
    wl_display_roundtrip(display);  // 获取输出信息

    // 确保处理所有事件，直到 done 被标记为 1
    // wl_display_dispatch(display);

    if (info.width > 0 && info.height > 0) {
        DEBUG_INFO("Current screen resolution: %dx%d\n", info.width, info.height);
    } else {
        DEBUG_INFO("Failed to retrieve screen resolution\n");
    }

    wl_display_disconnect(display);
    *width = info.width;
    *height = info.height;
    return 0;
}


#if 0
 int main() {
     struct wl_display *display = wl_display_connect(NULL);
     if (!display) {
         fprintf(stderr, "Failed to connect to Wayland display\n");
         return -1;
     }

     struct wl_registry *registry = wl_display_get_registry(display);
     struct display_info info = {0, 0};  // 初始化 done 为 0

     // wl_registry listener (注册监听器)
     static const struct wl_registry_listener registry_listener = {
         .global = registry_global_handler,
         .global_remove = registry_global_remove_handler
     };

     wl_registry_add_listener(registry, &registry_listener, &info);

     // 调用 wl_display_roundtrip() 以确保事件被处理
     wl_display_roundtrip(display);  // 获取注册对象
     wl_display_roundtrip(display);  // 获取输出信息

     // 确保处理所有事件，直到 done 被标记为 1
     // wl_display_dispatch(display);

     if (info.width > 0 && info.height > 0) {
         printf("Current screen resolution: %dx%d\n", info.width, info.height);
     } else {
         printf("Failed to retrieve screen resolution\n");
     }

     wl_display_disconnect(display);
     return 0;
 }
#endif
