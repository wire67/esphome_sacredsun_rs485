import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import CONF_ID
CODEOWNERS = ["@wire67"]
DEPENDENCIES = ["uart"]
sacredsun_ns = cg.esphome_ns.namespace("sacredsun")
SacredSunSensor = sacredsun_ns.class_("SacredSunSensor", cg.PollingComponent)
CONF_UART_ID = "uart_id"
CONF_PACK = "pack"
CONF_SENSORS = "sensors"
CONF_TYPE = "type"
SENSOR_TYPES = {
    "soc": "soc",
    "voltage": "voltage",
    "current": "current",
    "soh": "soh",
    "nominal_cap": "nominal_cap",
    "remaining_cap": "remaining_cap",
    "cycles": "cycles",
}
SENSOR_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES),
    }
)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SacredSunSensor),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional("send_interval", default=4000): cv.int_range(min=1000),
        cv.Optional("pack_count", default=2): cv.int_range(min=1, max=9),
    }
).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(
        config[cv.GenerateID()],
        uart_component,
        config["send_interval"],
        config["pack_count"],
    )
    await cg.register_component(var, config)