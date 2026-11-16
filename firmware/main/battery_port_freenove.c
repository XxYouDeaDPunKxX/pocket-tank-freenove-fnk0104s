/* Freenove FNK0104S optional battery monitor.
 * Freenove Sketch_05.1_Battery_Voltage: GPIO9 = ADC1 CH8, with a 1:2
 * resistor divider between the Li-ion cell and the ADC.
 *
 * The default firmware profile is USB-C only: no battery telemetry is shown.
 * With CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC and a fitted cell, the
 * voltage-derived percentage is approximate (especially under load);
 * GPIO9 cannot tell
 * whether USB power is present or the cell is charging. Never claim a PMIC,
 * switch any rail, or change charging currents on this board.
 */
#include "battery_port.h"
#include "board_pins.h"
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#endif
#include "esp_log.h"

static const char *TAG = "battery";
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static int64_t s_last_us = -1;
static int s_mv;
static bool s_valid;

/* A resting Li-ion voltage curve, deliberately coarse; no coulomb counter. */
static float level_from_mv(int mv) {
    static const struct { int mv, pct; } curve[] = {
        { 3250, 0 }, { 3500, 5 }, { 3600, 10 }, { 3700, 25 },
        { 3750, 40 }, { 3800, 55 }, { 3900, 75 },
        { 4000, 87 }, { 4100, 96 }, { 4200, 100 },
    };
    if (mv <= curve[0].mv) return 0;
    for (unsigned i = 1; i < sizeof curve / sizeof curve[0]; i++) {
        if (mv <= curve[i].mv) {
            const int span = curve[i].mv - curve[i - 1].mv;
            const int delta = mv - curve[i - 1].mv;
            return (curve[i - 1].pct + (float)delta * (curve[i].pct - curve[i - 1].pct) / span) / 100.0f;
        }
    }
    return 1;
}

static void sample(void) {
    if (!s_adc || !s_cali) return;
    const int64_t now = esp_timer_get_time();
    if (s_last_us >= 0 && now - s_last_us < 900000) return;
    s_last_us = now;

    int sum = 0, n = 0;
    for (int i = 0; i < 8; i++) {
        int raw;
        if (adc_oneshot_read(s_adc, ADC_CHANNEL_8, &raw) == ESP_OK) {
            sum += raw;
            n++;
        }
    }
    int pin_mv;
    if (!n || adc_cali_raw_to_voltage(s_cali, sum / n, &pin_mv) != ESP_OK) {
        s_valid = false;
        s_mv = 0;
        return;
    }
    s_mv = pin_mv * 2;                /* board's 1:2 divider */
    s_valid = s_mv >= 2500 && s_mv <= 4400;
    if (!s_valid) s_mv = 0;         /* no cell / impossible reading */
}
#endif

bool battery_port_init(i2c_master_bus_handle_t bus) {
    (void)bus;
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    esp_err_t err = adc_oneshot_new_unit(&unit, &s_adc);
    if (err == ESP_OK) {
        adc_oneshot_chan_cfg_t chan = {
            .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
        };
        err = adc_oneshot_config_channel(s_adc, ADC_CHANNEL_8, &chan);
    }
    if (err == ESP_OK) {
        adc_cali_curve_fitting_config_t cal = {
            .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_8,
            .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
        };
        err = adc_cali_create_scheme_curve_fitting(&cal, &s_cali);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "FNK0104S GPIO%d calibrated ADC unavailable: %s",
                 F_PIN_BAT_ADC, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "FNK0104S battery: GPIO%d ADC1_CH8, 1:2 divider; %% is an estimate, charging unknown",
                 F_PIN_BAT_ADC);
    }
#else
    ESP_LOGI(TAG, "FNK0104S USB-C-only profile: battery ADC disabled; USB presence is assumed, not sensed");
#endif
    return false; /* no AXP2101: main.c must use the BOOT GPIO for sleep */
}

bool battery_port_read(float *frac, bool *charging) {
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
    sample();
    if (!s_valid) return false;
    *frac = level_from_mv(s_mv);
    *charging = false;              /* ADC voltage alone cannot detect charge */
    return true;
#else
    (void)frac; (void)charging;
    return false;                    /* no cell: hide the battery gauge */
#endif
}
int battery_port_vbat_mv(void) {
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
    sample(); return s_valid ? s_mv : 0;
#else
    return 0;
#endif
}
int battery_port_state(void) {
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
    return BAT_ON_BATTERY;           /* voltage alone does not reveal a USB cable */
#else
    return BAT_PLUGGED;              /* USB-C is the declared power source in this build */
#endif
}
bool battery_port_poweroff(void) { return false; }
void battery_port_key_init(void) {}
int battery_port_key_poll(void) { return 0; }
void battery_port_key_clear(void) {}
void battery_port_key_trace(int seconds) {
    (void)seconds;
    ESP_LOGI(TAG, "FNK0104S uses BOOT GPIO0, not an AXP2101 PWR key");
}
void battery_port_dump(void) {
#ifdef CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC
    sample();
    if (s_valid)
        ESP_LOGI(TAG, "FNK0104S ADC battery %d mV, approx %d%%; USB/charge state unavailable",
                 s_mv, (int)(level_from_mv(s_mv) * 100 + 0.5f));
    else
        ESP_LOGI(TAG, "FNK0104S ADC battery absent or unavailable (GPIO%d)", F_PIN_BAT_ADC);
#else
    ESP_LOGI(TAG, "FNK0104S USB-C-only profile: no cell gauge; cable state is assumed");
#endif
}
int battery_port_charge_ma(void) { return 0; }
bool battery_port_set_charge_ma(int ma) { (void)ma; return false; }
bool battery_port_set_charger(bool on) { (void)on; return false; }
bool battery_port_set_cell(int mah) { (void)mah; return false; }
void battery_port_charge_report(void) {
    ESP_LOGI(TAG, "FNK0104S has no AXP2101 charge-current controls");
}
bool battery_port_set_rail(const char *name, bool on) {
    (void)name; (void)on;
    return false;
}
void battery_port_trim_rails(void) {}
void battery_port_pin_rail(const char *name) { (void)name; }
