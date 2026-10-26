import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import CONF_ID
CODEOWNERS = ["@wire67"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor"]
sacredsun_ns = cg.esphome_ns.namespace("sacredsun")
SacredSunSensor = sacredsun_ns.class_("SacredSunSensor", cg.PollingComponent)
CONF_UART_ID = "uart_id"
CONF_OFFLINE_TIMEOUT = "offline_timeout"
CONF_SOC_CORR_MAX_CURRENT = "soc_correction_max_current"
CONF_SOC_CORR_SLEW = "soc_correction_slew"
CONF_SOC_CORR_FULL_MV = "soc_correction_full_cell_mv"
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
    "remaining_kwh": "remaining_kwh",
    "soc_corrected": "soc_corrected",
    "cell_min": "cell_min",
    "cell_max": "cell_max",
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
        cv.Optional(CONF_OFFLINE_TIMEOUT, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SOC_CORR_MAX_CURRENT, default=10.0): cv.float_range(min=0.1, max=200),
        cv.Optional(CONF_SOC_CORR_SLEW, default=2.0): cv.float_range(min=0.1, max=100),
        cv.Optional(CONF_SOC_CORR_FULL_MV, default=3450): cv.int_range(min=3300, max=3700),
    }
).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(
        config[cv.GenerateID()],
        uart_component,
        config["send_interval"],
        config["pack_count"],
        config[CONF_OFFLINE_TIMEOUT],
    )
    cg.add(
        var.set_soc_correction(
            config[CONF_SOC_CORR_MAX_CURRENT],
            config[CONF_SOC_CORR_SLEW],
            config[CONF_SOC_CORR_FULL_MV],
        )
    )
    await cg.register_component(var, config)