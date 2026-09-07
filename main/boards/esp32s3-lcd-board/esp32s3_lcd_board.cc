#include "wifi_board.h"
#include "audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include <driver/i2c_master.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

#define TAG "Esp32S3LcdBoard"

class Esp32S3LcdBoard : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags = { .enable_internal_pullup = 1 };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        for (uint8_t addr = 1; addr < 127; addr++) {
            esp_err_t err = i2c_master_probe(i2c_bus_, addr, 100);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Audio codec I2C device found at 0x%02X", addr);
            }
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        // ST7735S 与 ST7789 命令集兼容，沿用项目已有先例
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeSdCard() {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;

        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4;
        slot.clk = SDMMC_CLK_GPIO;
        slot.cmd = SDMMC_CMD_GPIO;
        slot.d0 = SDMMC_D0_GPIO;
        slot.d1 = SDMMC_D1_GPIO;
        slot.d2 = SDMMC_D2_GPIO;
        slot.d3 = SDMMC_D3_GPIO;
        slot.cd = SDMMC_SLOT_NO_CD;
        slot.wp = SDMMC_SLOT_NO_WP;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
        };

        sdmmc_card_t* card = nullptr;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot, &mount_config, &card);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "SD card mount failed (no TF card inserted): %s", esp_err_to_name(ret));
            return;
        }
        sdmmc_card_print_info(stdout, card);
        ESP_LOGI(TAG, "SD card mounted at %s", SD_CARD_MOUNT_POINT);
    }

public:
     Esp32S3LcdBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeSdCard();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return nullptr;  // 背光常亮，无 GPIO 控制
    }
};

DECLARE_BOARD(Esp32S3LcdBoard);