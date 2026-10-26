import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from . import (
    SacredSunSensor,
    CONF_PACK,
    CONF_SENSORS,
    CONF_TYPE,
    SENSOR_SCHEMA,
)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID("parent"): cv.use_id(SacredSunSensor),
        cv.Optional(CONF_PACK, default=-1): cv.int_range(min=-1, max=9),
        cv.Required(CONF_SENSORS): cv.ensure_list(SENSOR_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
    parent = await cg.get_variable(config[cv.GenerateID("parent")])
    for sensor_config in config[CONF_SENSORS]:
        sensor_type = sensor_config[CONF_TYPE]
        var = await sensor.new_sensor(sensor_config)
        cg.add(parent.add_sensor(sensor_type, config[CONF_PACK], var))