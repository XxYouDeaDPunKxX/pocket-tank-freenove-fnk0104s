/* Freenove FNK0104S: ST7796 4-inch SPI TFT at 480x320 landscape.
 * Hardware pins are taken from Freenove's FNK0104S TFT_eSPI_Setups_v1.2.
 * The common game keeps its 448x368 logical frame so all pages retain their
 * original layout. This port samples it into 480x320 RGB565 DMA stripes.
 * Touch uses the inverse transform in freenove_geometry.h.
 *
 * Two DMA stripes let the CPU resample the next stripe while SPI sends the
 * previous one. A flush still completes before returning, as main.c expects. */
#include "display_port.h"
#include "freenove_geometry.h"
#include "render.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stddef.h>

static const char *TAG = "fnk0104s";
#define LCD_HOST       SPI2_HOST
#define LCD_PCLK_HZ    (80 * 1000 * 1000) /* Freenove's TFT_eSPI setting */
#define LCD_STRIPE_H   16
#define LCD_STRIPE_B   (FNK_GLASS_W * LCD_STRIPE_H * 2)
#define LCD_DMA_BUFFERS 2

static esp_lcd_panel_handle_t s_panel;
static i2c_master_bus_handle_t s_i2c;
static SemaphoreHandle_t s_dma_done;
static uint16_t *s_stripe[LCD_DMA_BUFFERS];
static bool s_ready, s_asleep, s_inverted;
static uint8_t s_brightness = 255;
static int64_t s_send_us, s_wait_us;

/* SPI completes color transfers in queue order. Each notification releases
 * the oldest stripe buffer; the framebuffer is safe when flush returns. */
static bool IRAM_ATTR color_sent(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *event, void *ctx) {
    (void)io; (void)event; (void)ctx;
    BaseType_t woke = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_done, &woke);
    return woke == pdTRUE;
}

static void backlight_apply(void) {
    if (!s_ready) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, s_asleep ? 0 : s_brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

bool display_port_init(void) {
    esp_err_t err;
    i2c_master_bus_config_t i2c = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = F_PIN_I2C_SDA,
        .scl_io_num = F_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    err = i2c_new_master_bus(&i2c, &s_i2c);
    if (err != ESP_OK) { ESP_LOGE(TAG, "I2C init: %s", esp_err_to_name(err)); return false; }

    /* Keep the panel dark until the driver and framebuffer are ready. */
    gpio_hold_dis(F_PIN_LCD_BL); /* release the pin held low over deep sleep */
    gpio_set_direction(F_PIN_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(F_PIN_LCD_BL, 0);
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&timer);
    if (err != ESP_OK) { ESP_LOGE(TAG, "backlight timer: %s", esp_err_to_name(err)); return false; }
    ledc_channel_config_t channel = {
        .gpio_num = F_PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) { ESP_LOGE(TAG, "backlight channel: %s", esp_err_to_name(err)); return false; }

    spi_bus_config_t spi = {
        .sclk_io_num = F_PIN_LCD_SCLK,
        .mosi_io_num = F_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_STRIPE_B + 8,
    };
    err = spi_bus_initialize(LCD_HOST, &spi, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) { ESP_LOGE(TAG, "LCD SPI bus: %s", esp_err_to_name(err)); return false; }

    s_dma_done = xSemaphoreCreateCounting(LCD_DMA_BUFFERS, 0);
    for (int i = 0; i < LCD_DMA_BUFFERS; i++)
        s_stripe[i] = heap_caps_malloc(LCD_STRIPE_B, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_dma_done || !s_stripe[0] || !s_stripe[1]) {
        ESP_LOGE(TAG, "no memory for LCD DMA stripes"); return false;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = F_PIN_LCD_CS,
        .dc_gpio_num = F_PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = LCD_DMA_BUFFERS,
        .on_color_trans_done = color_sent,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io);
    if (err != ESP_OK) { ESP_LOGE(TAG, "LCD SPI IO: %s", esp_err_to_name(err)); return false; }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,       /* panel reset is wired to the board's reset */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, /* Freenove TFT_eSPI ST7796 MADCTL order */
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7796(io, &panel_cfg, &s_panel);
    if (err != ESP_OK) { ESP_LOGE(TAG, "ST7796 create: %s", esp_err_to_name(err)); return false; }
    err = esp_lcd_panel_reset(s_panel);
    if (err == ESP_OK) err = esp_lcd_panel_init(s_panel);
    if (err == ESP_OK) err = esp_lcd_panel_swap_xy(s_panel, true);
    /* Freenove TFT_eSPI setRotation(1): MADCTL MV, without MX.  MX mirrored
     * the entire image horizontally on the physical FNK0104S. */
    if (err == ESP_OK) err = esp_lcd_panel_mirror(s_panel, false, false);
    if (err == ESP_OK) err = esp_lcd_panel_invert_color(s_panel, true); /* vendor TFT_INVERSION_ON */
    if (err == ESP_OK) err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) { ESP_LOGE(TAG, "ST7796 init: %s", esp_err_to_name(err)); return false; }

    s_ready = true;
    backlight_apply();
    ESP_LOGI(TAG, "ST7796 %dx%d landscape from %dx%d logical; FT6336U I2C 16/15",
             FNK_GLASS_W, FNK_GLASS_H, TANK_W, TANK_H);
    return true;
}

void display_port_flush(const uint16_t *fb) {
    if (!s_ready || !fb || s_asleep) return;
    const int64_t start = esp_timer_get_time();
    int queued = 0;
    int stripe_index = 0;
    for (int y0 = 0; y0 < FNK_GLASS_H; y0 += LCD_STRIPE_H) {
        /* A DMA buffer cannot be rewritten until its color transfer ends.
         * The panel's SPI queue and callbacks complete in submission order. */
        if (queued == LCD_DMA_BUFFERS) {
            const int64_t wait_start = esp_timer_get_time();
            if (xSemaphoreTake(s_dma_done, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGE(TAG, "LCD DMA timeout; disabling flush until reboot");
                s_ready = false;
                return;
            }
            s_wait_us += esp_timer_get_time() - wait_start;
            queued--;
        }
        const int rows = y0 + LCD_STRIPE_H <= FNK_GLASS_H ? LCD_STRIPE_H : FNK_GLASS_H - y0;
        uint16_t *stripe = s_stripe[stripe_index % LCD_DMA_BUFFERS];
        for (int row = 0; row < rows; row++) {
            int sy = fnk_frame_y(y0 + row);
            if (s_inverted) sy = TANK_H - 1 - sy;
            const uint16_t *src = fb + (size_t)sy * TANK_W;
            uint16_t *dst = stripe + (size_t)row * FNK_GLASS_W;
            for (int x = 0; x < FNK_GLASS_W; x++) {
                int sx = fnk_frame_x(x);
                dst[x] = src[s_inverted ? TANK_W - 1 - sx : sx];
                /* PT_FB_SWAP=1: the renderer already wrote wire-order RGB565. */
            }
        }
        const esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, y0,
                                                        FNK_GLASS_W, y0 + rows, stripe);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "draw stripe: %s; disabling flush until reboot", esp_err_to_name(err));
            s_ready = false;
            return;
        }
        queued++;
        stripe_index++;
    }
    while (queued--) {
        const int64_t wait_start = esp_timer_get_time();
        if (xSemaphoreTake(s_dma_done, pdMS_TO_TICKS(500)) != pdTRUE) {
            /* The stripe may still be in DMA: never reuse its storage. */
            ESP_LOGE(TAG, "LCD DMA timeout; disabling flush until reboot");
            s_ready = false;
            return;
        }
        s_wait_us += esp_timer_get_time() - wait_start;
    }
    s_send_us += esp_timer_get_time() - start;
}

void display_port_flush_prof(int64_t *wait_us, int64_t *send_us) {
    *wait_us = s_wait_us; *send_us = s_send_us;
    s_wait_us = s_send_us = 0;
}
bool display_port_flush_async(const uint16_t *fb, void (*done)(const uint16_t *, void *), void *arg) {
    (void)fb; (void)done; (void)arg; return false;
}
void display_port_flush_wait(const uint16_t *fb) { (void)fb; }
void display_port_flush_async_prof(int64_t *task_us, int *frames, int64_t *copy_us) {
    *task_us = 0; *frames = 0; *copy_us = 0;
}

void display_port_sleep(void) {
    s_asleep = true; backlight_apply();
    if (s_panel) esp_lcd_panel_disp_on_off(s_panel, false);
}
void display_port_wake(void) {
    if (s_panel) esp_lcd_panel_disp_on_off(s_panel, true);
    s_asleep = false; backlight_apply();
}
bool display_port_panel_power_check(void) { return false; }
uint32_t display_port_panel_heals(void) { return 0; }
void display_port_panel_rail_test_off(void) {}
void display_port_set_inverted(bool inverted) { s_inverted = inverted; }
void display_port_set_quarter(int q) { s_inverted = (q & 2) != 0; }
bool display_port_quarters(void) { return false; }
void display_port_side_tune(int madctl, int gx, int gy) { (void)madctl; (void)gx; (void)gy; }
void display_port_side_info(int *madctl, int *gx, int *gy, bool *on) {
    *madctl = *gx = *gy = 0; *on = false;
}
void display_port_probe(bool on) { (void)on; }
void display_port_set_brightness(uint8_t level) { s_brightness = level; backlight_apply(); }
uint8_t display_port_brightness(void) { return s_brightness; }

i2c_master_bus_handle_t board_i2c_bus(void) { return s_i2c; }
bool board_is_v2(void) { return false; }
bool board_is_round(void) { return false; }
bool board_is_175(void) { return false; }
bool board_is_watch(void) { return false; }
bool board_is_square(void) { return false; }
bool board_is_freenove(void) { return true; }
bool board_has_expander(void) { return false; }
uint8_t board_es7210_addr(void) { return 0; }
const char *board_port_hw(void) { return "fnk0104s"; }
bool board_port_probed(void) { return true; }
int board_pwr_sense_pin(void) { return -1; }
void board_sd_select(bool on) { (void)on; }

void display_port_deep_sleep_pins(bool tp_awake_high) {
    (void)tp_awake_high;
    /* Don't leave the backlight input floating over an ESP32 deep sleep. */
    gpio_reset_pin(F_PIN_LCD_BL);
    gpio_set_direction(F_PIN_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(F_PIN_LCD_BL, 0);
    gpio_hold_en(F_PIN_LCD_BL);
}
void display_port_deep_sleep_bus(void) {}
void display_port_deep_standby(void) {}
void display_port_set_madctl(uint8_t v) { (void)v; }
uint8_t display_port_madctl(void) { return 0; }
void display_port_frame_origin(int *px, int *py) { *px = *py = 0; }
void display_port_set_view(int view) { (void)view; }
int display_port_view(void) { return DISPLAY_VIEW_FIT; }
void display_port_panel_to_tank(int px, int py, float *tx, float *ty) {
    *tx = (float)fnk_touch_x(py); *ty = (float)fnk_touch_y(px);
}
