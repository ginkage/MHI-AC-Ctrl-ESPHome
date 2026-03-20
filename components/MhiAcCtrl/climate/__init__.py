# __init__.py

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.core import coroutine
from esphome.const import (
    CONF_ID,
)
from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace('mhi')
MhiClimate = mhi_ns.class_('MhiClimate', cg.Component, climate.Climate)

CONF_TEMPERATURE_OFFSET = "temperature_offset"
CONF_VISUAL_MIN_TEMPERATURE = "visual_min_temperature"

CONFIG_SCHEMA = climate.climate_schema(MhiClimate).extend(
    {
        cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
        cv.Optional(CONF_TEMPERATURE_OFFSET, default=False): cv.boolean,
        cv.Optional(CONF_VISUAL_MIN_TEMPERATURE, default=18.0): cv.temperature,
        # Diese Zeile erzwingt, dass kein Icon-Code generiert wird, der set_icon aufruft:
        cv.Optional("icon"): cv.invalid("Icons werden für diese Komponente in 2026.3 via C++ nicht unterstützt"),
    }
).extend(cv.COMPONENT_SCHEMA)




@coroutine
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    await cg.register_parented(var, mhi)
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    
    if config[CONF_TEMPERATURE_OFFSET]:
        cg.add(var.set_temperature_offset_enabled(True))

    if CONF_VISUAL_MIN_TEMPERATURE in config:
        cg.add(var.set_minimum_temperature(config[CONF_VISUAL_MIN_TEMPERATURE]))
