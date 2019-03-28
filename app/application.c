#include <application.h>

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;
bc_button_t lcd_left;
bc_button_t lcd_right;

bc_gfx_t *gfx;

bc_soil_sensor_t soil;

typedef enum
{
    STATE_SCAN = 0,
    STATE_READ_0,
    STATE_READ_100,
    STATE_CONFIRM,
    STATE_SAVE,
    STATE_DONE
} state_t;

state_t state = STATE_SCAN;
int scan_cnt = 0;
uint64_t sensor_device_address;
uint16_t raw;
uint16_t raw_min;
uint16_t raw_max;
bool raw_valid;

void reset(void)
{
    state = STATE_SCAN;
    raw_min = 0xffff;
    raw_max = 0;
    raw_valid = false;
}

void soil_sensor_event_handler(bc_soil_sensor_t *self, uint64_t device_address, bc_soil_sensor_event_t event, void *event_param)
{
    if (event == BC_SOIL_SENSOR_EVENT_UPDATE)
    {
        raw_valid = bc_soil_sensor_get_cap_raw(self, device_address, &raw);

        if (state == STATE_SCAN)
        {
            state = STATE_READ_0;
            sensor_device_address = device_address;
        }
    }
    else if (event == BC_SOIL_SENSOR_EVENT_ERROR)
    {
        bc_log_error("Soil error: %d", bc_soil_sensor_get_error(self));

        reset();
    }
}

void button_left_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        if (state == STATE_READ_0)
        {
            reset();
        }
        else if (state == STATE_READ_100)
        {
            state = STATE_READ_0;
        }
        else if (state == STATE_CONFIRM)
        {
            state = STATE_READ_100;
        }

        bc_scheduler_plan_now(0);
    }
}

void button_right_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        if (state == STATE_READ_0)
        {
            if (raw_valid)
            {
                raw_min = raw;
                state = STATE_READ_100;
            }
        }
        else if (state == STATE_READ_100)
        {
            if (raw_valid){
                raw_max = raw;
                state = STATE_CONFIRM;
            }
        }
        else if (state == STATE_CONFIRM)
        {
            state = STATE_SAVE;
        }

        bc_scheduler_plan_now(0);
    }
}

// void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
// {
//     if (event == BC_BUTTON_EVENT_PRESS)
//     {
//         bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
//     }

//     // Logging in action
//     bc_log_info("Button event handler - event: %i", event);
// }

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);


    bc_module_lcd_init();

    // // Initialize button
    // bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    // bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize LCD button left
    bc_button_init_virtual(&lcd_left, BC_MODULE_LCD_BUTTON_LEFT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_debounce_time(&lcd_left, 20);
    bc_button_set_event_handler(&lcd_left, button_left_event_handler, NULL);

    // Initialize LCD button right
    bc_button_init_virtual(&lcd_right, BC_MODULE_LCD_BUTTON_RIGHT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_debounce_time(&lcd_right, 20);
    bc_button_set_event_handler(&lcd_right, button_right_event_handler, NULL);

    gfx = bc_module_lcd_get_gfx();

    bc_soil_sensor_init(&soil);
    bc_soil_sensor_set_event_handler(&soil, soil_sensor_event_handler, NULL);
    bc_soil_sensor_set_update_interval(&soil, 200);

    reset();

    bc_led_pulse(&led, 2000);
}

void application_task(void)
{
    if (!bc_gfx_display_is_ready(gfx))
    {
        bc_scheduler_plan_current_now();
        return;
    }

    bc_gfx_clear(gfx);

    switch (state)
    {
        case STATE_SCAN:
        {
            bc_gfx_set_font(gfx, &bc_font_ubuntu_24);
            bc_gfx_draw_string(gfx, 5, 5, "Calibration", 1);
            bc_gfx_draw_string(gfx, 5, 30, "Soil Sensor", 1);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);

            int left = bc_gfx_draw_string(gfx, 10, 60, "scan ", 1);

            for (int i = 0; i <= scan_cnt; i++)
            {
                left += bc_gfx_draw_char(gfx, left, 60, '.', 1);
            }

            if (++scan_cnt == 15)
            {
                scan_cnt = 0;
            }

            bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
            bc_gfx_draw_string(gfx, 1, 100, "connect sensor please", 1);

            break;
        }
        case STATE_READ_0:
        {
            bc_gfx_draw_string(gfx, 5, 5, "For: 0%", 1);
            bc_gfx_draw_string(gfx, 10, 20, "Dry", 1);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_24);
            bc_gfx_printf(gfx, 30, 50, 1, "%d", raw);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_printf(gfx, 0, 90, 1, "%016llx", sensor_device_address);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
            bc_gfx_draw_string(gfx, 5, 110, "[back]", 1);
            bc_gfx_draw_string(gfx, 90, 110, "[next]", 1);

            break;
        }
        case STATE_READ_100:
        {
            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_draw_string(gfx, 5, 5, "For: 100%", 1);
            bc_gfx_draw_string(gfx, 10, 20, "Submerged", 1);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_24);
            bc_gfx_printf(gfx, 30, 50, 1, "%d", raw);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_printf(gfx, 0, 90, 1, "%016llx", sensor_device_address);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
            bc_gfx_draw_string(gfx, 5, 110, "[back]", 1);
            bc_gfx_draw_string(gfx, 90, 110, "[next]", 1);

            break;
        }
        case STATE_CONFIRM:
        {
            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_draw_string(gfx, 5, 5, "Confirm", 1);

            bc_gfx_printf(gfx, 5, 20, 1, "  0%%: %d", raw_min);
            bc_gfx_printf(gfx, 5, 40, 1, "100%%: %d", raw_max);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_printf(gfx, 0, 90, 1, "%016llx", sensor_device_address);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
            bc_gfx_draw_string(gfx, 5, 110, "[back]", 1);
            bc_gfx_draw_string(gfx, 90, 110, "[next]", 1);

            break;
        }
        case STATE_SAVE:
        {
            bc_gfx_set_font(gfx, &bc_font_ubuntu_15);
            bc_gfx_draw_string(gfx, 5, 5, "Save to eeprom", 1);

            bc_gfx_draw_string(gfx, 5, 25, "wait please", 1);

            bc_gfx_update(gfx);

            uint16_t half = ((raw_max - raw_min) / 2) + raw_min;
            uint16_t value;

            uint16_t step = (half - raw_min) / 5;

            for (int i = 0; i < 5; i++)
            {
                value = raw_min + i * step;

                bc_log_debug("%i %d", i * 10, value);
                bc_soil_sensor_calibration_set_point(&soil, sensor_device_address, i * 10, value);
            }

            step = (raw_max - half) / 5;

            for (int i = 0; i < 6; i++)
            {
                value = half + i * step;

                bc_log_debug("%i %d", (i + 5) * 10, value);
                bc_soil_sensor_calibration_set_point(&soil, sensor_device_address, (i + 5) * 10, value);
            }

            if (!bc_soil_sensor_eeprom_save(&soil, sensor_device_address))
            {
                bc_log_error("bc_soil_sensor_eeprom_save");
            }

            state = STATE_DONE;

            break;
        }
        case STATE_DONE:
        {
            bc_gfx_set_font(gfx, &bc_font_ubuntu_24);
            bc_gfx_draw_string(gfx, 5, 5, "Complete", 1);

            bc_gfx_set_font(gfx, &bc_font_ubuntu_13);
            bc_gfx_draw_string(gfx, 5, 40, "connect new sensor", 1);

            break;
        }
        default:
            break;
    }

    bc_gfx_update(gfx);

    bc_scheduler_plan_current_from_now(100);
}
