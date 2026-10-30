import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from esphome.const import (
    DEVICE_CLASS_SWITCH
)

from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace('mhi')
Mhi3dAutoSwitch = mhi_ns.class_('Mhi3dAutoSwitch', switch.Switch, cg.Component)
MhiSilentOperationSwitch = mhi_ns.class_('MhiSilentOperationSwitch', switch.Switch, cg.Component)
MhiPowerSwitch = mhi_ns.class_('MhiPowerSwitch', switch.Switch, cg.Component)

CONF_POWER = "power"
CONF_VANES_3D_AUTO = "vanes_3d_auto"
CONF_SILENT_OPERATION = "silent_operation"
ICON_3D="mdi:video-3d"

CONFIG_SCHEMA = cv.Schema({    
    cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    cv.Optional(CONF_POWER): switch.switch_schema(
        MhiPowerSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon="mdi:power",
    ),
    cv.Optional(CONF_VANES_3D_AUTO): switch.switch_schema(
        Mhi3dAutoSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon=ICON_3D,
    ),
    cv.Optional(CONF_SILENT_OPERATION): switch.switch_schema(
        MhiSilentOperationSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon="mdi:fan-chevron-down",
    ),
})

async def to_code(config):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    if power_config := config.get(CONF_POWER):
        s = await switch.new_switch(power_config)
        await cg.register_component(s, power_config)
        await cg.register_parented(s, mhi)
    if vanes_3d_auto_config := config.get(CONF_VANES_3D_AUTO):
        s = await switch.new_switch(vanes_3d_auto_config)
        await cg.register_component(s, vanes_3d_auto_config)
        await cg.register_parented(s, mhi)
    if silent_operation_config := config.get(CONF_SILENT_OPERATION):
        s = await switch.new_switch(silent_operation_config)
        await cg.register_component(s, silent_operation_config)
        await cg.register_parented(s, mhi)
