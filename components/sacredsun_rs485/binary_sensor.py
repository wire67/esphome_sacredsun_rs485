import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC
from . import SacredSunSensor, CONF_PACK

CONF_SACREDSUN_ID = "sacredsun_rs485_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_CONNECTIVITY,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_SACREDSUN_ID): cv.use_id(SacredSunSensor),
        cv.Required(CONF_PACK): cv.int_range(min=0, max=8),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SACREDSUN_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(parent.set_online_sensor(config[CONF_PACK], var))
