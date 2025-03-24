import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from esphome.const import (
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_ENERGY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_HOUR,
    UNIT_HERTZ,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    ICON_THERMOMETER,
    ICON_FAN,
    ICON_TIMER,
    ENTITY_CATEGORY_CONFIG,
    ICON_BLUETOOTH,
    DEVICE_CLASS_SWITCH
)

from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace('mhi')
Mhi3dAutoSwitch = mhi_ns.class_('Mhi3dAutoSwitch', switch.Switch, cg.Component)

CONF_VANES_3D_AUTO = "vanes_3d_auto"
ICON_3D="mdi:video-3d"

CONFIG_SCHEMA = cv.Schema({    
    cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    cv.Optional(CONF_VANES_3D_AUTO): switch.switch_schema(
        Mhi3dAutoSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon=ICON_3D,
    ),
})

async def to_code(config):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    if vanes_3d_auto_config := config.get(CONF_VANES_3D_AUTO):
        s = await switch.new_switch(vanes_3d_auto_config)
        await cg.register_component(s, vanes_3d_auto_config)
        await cg.register_parented(s, mhi)