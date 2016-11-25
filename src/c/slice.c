#include <pebble.h>
#include <pebble-events/pebble-events.h>
#include <pebble-connection-vibes/connection-vibes.h>
#include <pebble-hourly-vibes/hourly-vibes.h>
#include "logging.h"
#include "enamel.h"

static Window *s_window;
static Layer *s_hour_layer;
static Layer *s_minute_layer;
static Layer *s_center_layer;

static uint16_t s_hour_degree;
static int32_t s_min_angle;
static GFont s_font;

static EventHandle s_tick_timer_event_handle;
static EventHandle s_settings_event_handle;

static inline GRect grect_from_point(GPoint point, GSize size) {
    log_func();
    return (GRect) {
        .origin = (GPoint) {
            .x = point.x - size.w / 2,
            .y = point.y - size.h / 2
        },
        .size = size
    };
}

static void settings_handler(void *context) {
    log_func();
    connection_vibes_set_state(atoi(enamel_get_CONNECTION_VIBE()));
    hourly_vibes_set_enabled(enamel_get_HOURLY_VIBE());
#ifdef PBL_HEALTH
    connection_vibes_enable_health(enamel_get_ENABLE_HEALTH());
    hourly_vibes_enable_health(enamel_get_ENABLE_HEALTH());
#endif
    layer_mark_dirty(window_get_root_layer(s_window));
}

static void hour_update_proc(Layer *layer, GContext *ctx) {
    log_func();
    GRect bounds = layer_get_bounds(layer);
    GRect crop = grect_crop(bounds, PBL_IF_ROUND_ELSE(15, 10));
    GSize size = GSize(25, 25);

#ifdef PBL_RECT
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_radial(ctx, grect_crop(bounds, 1), GOvalScaleModeFitCircle, bounds.size.w / 2, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(360));
#endif

    graphics_context_set_text_color(ctx, GColorBlack);
    for (int i = 1; i <= 12; i++) {
        int32_t angle = TRIG_MAX_ANGLE * i / 12;
        GRect rect = grect_centered_from_polar(crop, GOvalScaleModeFitCircle, angle, size);
        char buf[3];
        snprintf(buf, sizeof(buf), "%d", i);
        graphics_draw_text(ctx, buf, s_font, rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }

    graphics_context_set_fill_color(ctx, GColorBlack);
    if (s_hour_degree > 0 && s_hour_degree < 360) {
        graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w / 2, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(s_hour_degree - 10));
        graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w / 2, DEG_TO_TRIGANGLE(s_hour_degree + 10), DEG_TO_TRIGANGLE(360));
    } else {
        graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w / 2, DEG_TO_TRIGANGLE(10), DEG_TO_TRIGANGLE(350));
    }
}

static void minute_update_proc(Layer *layer, GContext *ctx) {
    log_func();
    GRect bounds = layer_get_bounds(layer);
    GPoint point = gpoint_from_polar(bounds, GOvalScaleModeFitCircle, s_min_angle);

    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 6);
    graphics_draw_line(ctx, grect_center_point(&bounds), point);

    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(enamel_get_COLOR_MINUTE_HAND(), GColorWhite));
    graphics_context_set_stroke_width(ctx, 4);
    graphics_draw_line(ctx, grect_center_point(&bounds), point);
}

static void center_update_proc(Layer *layer, GContext *ctx) {
    log_func();
    GRect bounds = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(enamel_get_COLOR_MINUTE_HAND(), GColorWhite));
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w, 0, DEG_TO_TRIGANGLE(360));

    bounds = grect_from_point(grect_center_point(&bounds), (GSize) { .w = 5, .h = 5 });
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w, 0, DEG_TO_TRIGANGLE(360));
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    log_func();
#ifndef DEMO
    s_hour_degree = (tick_time->tm_hour > 12 ? tick_time->tm_hour % 12 : tick_time->tm_hour) * 30;
    s_min_angle = TRIG_MAX_ANGLE * tick_time->tm_min / 60;
#else
    s_hour_degree = 11 * 30;
    s_min_angle = TRIG_MAX_ANGLE * 10 / 60;
#endif
    layer_mark_dirty(window_get_root_layer(s_window));
}

static void window_load(Window *window) {
    log_func();
    window_set_background_color(window, PBL_IF_ROUND_ELSE(GColorWhite, GColorBlack));

    Layer *root_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root_layer);

    s_hour_layer = layer_create(bounds);
    layer_set_update_proc(s_hour_layer, hour_update_proc);
    layer_add_child(root_layer, s_hour_layer);

    s_minute_layer = layer_create(grect_crop(bounds, PBL_IF_ROUND_ELSE(30, 20)));
    layer_set_update_proc(s_minute_layer, minute_update_proc);
    layer_add_child(root_layer, s_minute_layer);

    s_center_layer = layer_create(grect_from_point(grect_center_point(&bounds), (GSize) { .w = 11, .h = 11 }));
    layer_set_update_proc(s_center_layer, center_update_proc);
    layer_add_child(root_layer, s_center_layer);

    time_t now = time(NULL);
    tick_handler(localtime(&now), MINUTE_UNIT);
    s_tick_timer_event_handle = events_tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    settings_handler(NULL);
    s_settings_event_handle = enamel_settings_received_subscribe(settings_handler, NULL);
}

static void window_unload(Window *window) {
    log_func();
    enamel_settings_received_unsubscribe(s_settings_event_handle);
    events_tick_timer_service_unsubscribe(s_tick_timer_event_handle);

    layer_destroy(s_center_layer);
    layer_destroy(s_minute_layer);
    layer_destroy(s_hour_layer);
}

static void init(void) {
    log_func();
    enamel_init();
    connection_vibes_init();
    hourly_vibes_init();
    uint32_t const pattern[] = { 100 };
    hourly_vibes_set_pattern((VibePattern) {
        .durations = pattern,
        .num_segments = 1
    });

    events_app_message_open();

    s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GILROY_22));

    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });
    window_stack_push(s_window, true);
}

static void deinit(void) {
    log_func();
    window_destroy(s_window);

    fonts_unload_custom_font(s_font);

    hourly_vibes_deinit();
    connection_vibes_deinit();
    enamel_deinit();
}

int main(void) {
    log_func();
    init();
    app_event_loop();
    deinit();
}
