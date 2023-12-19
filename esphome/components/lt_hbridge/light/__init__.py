from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID, CONF_PIN_A, CONF_PIN_B
from .. import lt_hbridge_ns

CODEOWNERS = ["@LeoDJ"]

DEPENDENCIES = ["libretiny"]

lt_hbridge_ns = cg.esphome_ns.namespace("lt_hbridge")

LTHBridgeLightOutput = lt_hbridge_ns.class_(
    "LTHBridgeLightOutput", cg.Component, light.LightOutput
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LTHBridgeLightOutput),
        cv.Required(CONF_PIN_A): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_PIN_B): pins.internal_gpio_output_pin_schema,
    }
)

async def to_code(config):
    gpio_a = await cg.gpio_pin_expression(config[CONF_PIN_A])
    gpio_b = await cg.gpio_pin_expression(config[CONF_PIN_B])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], gpio_a, gpio_b)
    await cg.register_component(var, config)
    await light.register_light(var, config)