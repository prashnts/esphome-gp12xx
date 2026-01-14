import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import (
    CONF_ID,
    CONF_PAGES,
    CONF_BRIGHTNESS,
    CONF_INVERT,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_RESET_PIN,
)
from esphome import pins

DEPENDENCIES = ["spi"]

CONF_FILAMENT_PIN = "filament_pin"

gp12xx = cg.esphome_ns.namespace("gp12xx")
GP12XX = gp12xx.class_(
    "GP12XX", cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice
)

GP12XXModel = gp12xx.enum("GP12XXModel")
MODELS = {
    "GP1294_256X48": GP12XXModel.GP1294_MODEL_256_48,
    "GP1287_256X50": GP12XXModel.GP1287_MODEL_256_50,
}
GP12XX_MODEL = cv.enum(MODELS, upper=True, space="_")

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GP12XX),
            cv.Required(CONF_MODEL): GP12XX_MODEL,
            cv.Optional(CONF_FILAMENT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
            cv.Optional(CONF_OFFSET_X, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_OFFSET_Y, default=0): cv.int_range(min=0, max=127),
            cv.Optional(CONF_INVERT, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "gp12xx", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    if filament_pin := config.get(CONF_FILAMENT_PIN):
        filament = await cg.gpio_pin_expression(filament_pin)
        cg.add(var.set_filament_pin(filament))

    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    if brightness := config.get(CONF_BRIGHTNESS):
        cg.add(var.init_brightness(brightness))

    if offset_x := config.get(CONF_OFFSET_X):
        cg.add(var.init_offset_x(offset_x))

    if offset_y := config.get(CONF_OFFSET_Y):
        cg.add(var.init_offset_y(offset_y))

    if CONF_INVERT in config:
        cg.add(var.init_invert(config[CONF_INVERT]))

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    await spi.register_spi_device(var, config)
