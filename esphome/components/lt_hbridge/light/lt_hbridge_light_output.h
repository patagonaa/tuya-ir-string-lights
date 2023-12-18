#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/log.h"

#ifdef USE_LIBRETINY
#include "wiring_private.h"

namespace esphome {
namespace lt_hbridge {

class LTHBridgeLightOutput : public Component, public light::LightOutput {
 public:
  LTHBridgeLightOutput(InternalGPIOPin *pin_a, InternalGPIOPin *pin_b) : pin_a_(pin_a), pin_b_(pin_b) {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(170); // chosen so the color temperature slider is divided roughly 50/50
    traits.set_max_mireds(200);
    return traits;
  }

  #define GPIO_INDEX uint8_t

  // copied from libretiny wiring_analog.c
  static inline constexpr const GPIO_INDEX pwmToGpio[] = {
    GPIO6,	// PWM0
    GPIO7,	// PWM1
    GPIO8,	// PWM2
    GPIO9,	// PWM3
    GPIO24, // PWM4
    GPIO26, // PWM5
  };

  static uint8_t gpioToPwm(GPIO_INDEX gpio) {
    for (uint8_t i = 0; i < sizeof(pwmToGpio) / sizeof(GPIO_INDEX); i++) {
      if (pwmToGpio[i] == gpio)
        return i;
    }
    return 0;
  }

  uint8_t pwmPinSetup(pin_size_t pinNumber, bool inverted) {
    pinCheckGetData(pinNumber, PIN_PWM, 255);  // weird macro that sets pin and data https://github.com/libretiny-eu/libretiny/blob/master/cores/common/arduino/src/wiring/wiring_private.h#L37

    // GPIO can't be used together with PWM
    pinRemoveMode(pin, PIN_GPIO | PIN_IRQ);
    
	  uint32_t cycles = 26 * (1e6 / freq_) - 1;

    if (!pinEnabled(pin, PIN_PWM)) {
		  pinEnable(pin, PIN_PWM);
    }

    pwm.channel		        = gpioToPwm(pin->gpio);
		pwm.cfg.bits.en	      = PWM_ENABLE;
		pwm.cfg.bits.int_en   = PWM_INT_DIS;
		pwm.cfg.bits.mode	    = PWM_PWM_MODE;
		pwm.cfg.bits.clk	    = PWM_CLK_26M;
		pwm.end_value		      = cycles;
    pwm.duty_cycle1       = !inverted ? cycles : 1;  // both outputs need to be low at boot
		pwm.p_Int_Handler	    = NULL;
    data->pwm = &pwm;

    ESP_LOGD(TAG, "pin: %d, chan: %d, d.p.c: %d, dc1: %d, ev: %d", pin->gpio, gpioToPwm(pin->gpio), data->pwm->channel, data->pwm->duty_cycle1, data->pwm->end_value);

    // PWM sddev handler: https://github.com/libretiny-eu/framework-beken-bdk/blob/platformio/beken378/driver/pwm/pwm_bk7231n.c#L740
    __wrap_bk_printf_disable();
    sddev_control(PWM_DEV_NAME, CMD_PWM_INIT_PARAM, data->pwm);
    if (!inverted)
      sddev_control(PWM_DEV_NAME, CMD_PWM_INIT_LEVL_SET_LOW, &data->pwm->channel);
    else
      sddev_control(PWM_DEV_NAME, CMD_PWM_INIT_LEVL_SET_HIGH, &data->pwm->channel);
    // sddev_control(PWM_DEV_NAME, CMD_PWM_UNIT_ENABLE, &data->pwm->channel); // done separately, for better synchronization
    __wrap_bk_printf_enable();
    return pwm.channel;
  }

  void pwmWrite(pin_size_t pinNumber, float val) {
    pinCheckGetData(pinNumber, PIN_PWM, );  // weird macro that sets pin and data https://github.com/libretiny-eu/libretiny/blob/master/cores/common/arduino/src/wiring/wiring_private.h#L37

	  uint32_t dutyCycle = val * (data->pwm->end_value);
    if (dutyCycle == 0) {
      // If the inverted output goes to zero, it "forgets" its inversion. 
      // So just set it to 1 (1/26000 PWM interval is basically invisible).
      dutyCycle = 1;  
    }

    data->pwm->channel = gpioToPwm(pin->gpio);
    data->pwm->duty_cycle1 = dutyCycle;  // only works for BK7231N

    __wrap_bk_printf_disable();
    sddev_control(PWM_DEV_NAME, CMD_PWM_SET_DUTY_CYCLE, data->pwm);
    __wrap_bk_printf_enable();
  }

  void write_state(light::LightState *state) override {
    state->current_values_as_cwww(&this->cw_, &this->ww_, false);

    float sum = cw_ + ww_;
    float limit_fact = 1.0;
    if (sum > 0.98) {
      limit_fact = (1 / sum) * 0.98;
    }
    // ESP_LOGD(TAG, "Brght: %.3f, CW: %.3f, WW: %.3f", sum, cw_, ww_);
    pwmWrite(this->pin_a_->get_pin(), 1.0 - (this->cw_ * limit_fact));
    pwmWrite(this->pin_b_->get_pin(), (this->ww_ * limit_fact));
  }

  void setup() override { 
    uint8_t chan_a = pwmPinSetup(this->pin_a_->get_pin(), false);
    uint8_t chan_b = pwmPinSetup(this->pin_b_->get_pin(), true);
    // start both PWM channels as fast as possible, to achieve an almost synchronous output (11us overlap)
    sddev_control(PWM_DEV_NAME, CMD_PWM_UNIT_ENABLE, &chan_a);
    sddev_control(PWM_DEV_NAME, CMD_PWM_UNIT_ENABLE, &chan_b);
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  InternalGPIOPin *pin_a_, *pin_b_;
  pwm_param_t pwm = {0};
  float cw_ = 0;
  float ww_ = 0;
  static const int freq_ = 1000;
  static inline constexpr const char* TAG = "LT_HBridge";
};

}  // namespace lt_hbridge
}  // namespace esphome

#endif