import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.core import coroutine
from . import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MhiAcCtrl),
        cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine
def to_code(config):
    paren = yield cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    yield climate.register_climate(paren, config)
