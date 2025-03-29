import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.core import coroutine
from esphome.const import (
    CONF_ID,
    ICON_AIR_CONDITIONER,  # Add this import
)
from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace('mhi')
MhiClimate = mhi_ns.class_('MhiClimate', cg.Component, climate.Climate)

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MhiClimate),
        cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    }
).extend(cv.COMPONENT_SCHEMA).extend(
    {
        cv.Optional("icon", default=ICON_AIR_CONDITIONER): cv.icon,  # Add icon configuration
    }
)


@coroutine
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    cg.add(var.set_icon(config["icon"]))  # Set the icon for the climate entity