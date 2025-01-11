/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "esp32/esp32_adc.h"

#include <stdbool.h>

//#include "driver/adc.h"
//
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

#include "common/platform.h"

#include "mgos_adc.h"
#include "mgos_sys_config.h"

static int s_vref = 1100;
static int s_width = ADC_BITWIDTH_12;

struct esp32_adc_channel_info {
  int pin;
  adc_channel_t ch;
  adc_atten_t atten;

  adc_oneshot_unit_handle_t handle;
  adc_cali_handle_t cali_h;
};

static struct esp32_adc_channel_info s_chans[8] = {
    {.pin = 0, .ch = ADC_CHANNEL_0, .atten = ADC_ATTEN_DB_12},
    {.pin = 1, .ch = ADC_CHANNEL_1, .atten = ADC_ATTEN_DB_12},
    {.pin = 2, .ch = ADC_CHANNEL_2, .atten = ADC_ATTEN_DB_12},
    {.pin = 3, .ch = ADC_CHANNEL_3, .atten = ADC_ATTEN_DB_12},
    {.pin = 4, .ch = ADC_CHANNEL_4, .atten = ADC_ATTEN_DB_12},
};

static struct esp32_adc_channel_info *esp32_adc_get_channel_info(int pin) {
  for (int i = 0; i < ARRAY_SIZE(s_chans); i++) {
    if (s_chans[i].pin == pin) return &s_chans[i];
  }
  return NULL;
}

static bool esp32_update_channel_settings(struct esp32_adc_channel_info *ci) {
  adc_oneshot_unit_init_cfg_t uconfig = {.unit_id = ADC_UNIT_1};

  // TODO: leaking handle, need to call adc_oneshot_del_unit
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&uconfig, &ci->handle));


  adc_oneshot_chan_cfg_t config = {
      .bitwidth = s_width,
      .atten = ci->atten,
  };

  if (adc_oneshot_config_channel(ci->handle, ci->ch, &config) != ESP_OK) {
    return false;
  }

  adc_cali_curve_fitting_config_t sconfig;
  sconfig.unit_id = ADC_UNIT_1;
  sconfig.chan = ci->ch;
  sconfig.atten = ci->atten;
  sconfig.bitwidth = s_width;
  // sconfig->default_vref = s_vref;

  adc_cali_create_scheme_curve_fitting(&sconfig, &ci->cali_h);

  return true;
}

bool esp32_set_channel_attenuation(int pin, int atten) {
  struct esp32_adc_channel_info *ci = esp32_adc_get_channel_info(pin);
  if (ci == NULL) return false;

  ci->atten = (adc_atten_t) atten;
  return esp32_update_channel_settings(ci);
}

bool mgos_adc_enable(int pin) {
  struct esp32_adc_channel_info *ci = esp32_adc_get_channel_info(pin);
  if (ci == NULL) return false;

  return esp32_update_channel_settings(ci);
}

int mgos_adc_read(int pin) {
  struct esp32_adc_channel_info *ci = esp32_adc_get_channel_info(pin);
  if (ci == NULL) return false;
  int raw = 0;
  ESP_ERROR_CHECK(adc_oneshot_read(ci->handle, ci->ch, &raw));
  return raw;
}

int mgos_adc_read_voltage(int pin) {
  struct esp32_adc_channel_info *ci = esp32_adc_get_channel_info(pin);
  if (ci == NULL) return false;
  int voltage;
  int raw;
  ESP_ERROR_CHECK(adc_oneshot_read(ci->handle, ci->ch, &raw));
  adc_cali_raw_to_voltage(ci->cali_h, raw, &voltage);

  return voltage;
}

void esp32_adc_set_vref(int vref_mv) {
  s_vref = vref_mv;
}

void esp32_adc_set_width(int width) {
  if ((width >= 0) && (width <= ADC_BITWIDTH_13)) {
    s_width = width;
  }
  // TODO: update all channels?
}

bool mgos_adc_init(void) {
  if (mgos_sys_config_get_sys_esp32_adc_vref() > 0) {
    esp32_adc_set_vref(mgos_sys_config_get_sys_esp32_adc_vref());
  }
  esp32_adc_set_width(mgos_sys_config_get_sys_esp32_adc_width());
  return true;
}
