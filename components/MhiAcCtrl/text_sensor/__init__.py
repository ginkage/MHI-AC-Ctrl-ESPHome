import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_ID
)
from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace('mhi')
MhiTextSensors = mhi_ns.class_('MhiTextSensors', cg.Component)

CONF_PROTECTION_STATE = "protection_state"

ICON_ALERT_OUTLINE = "mdi:shield-alert-outline"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MhiTextSensors),
    cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    cv.Optional(CONF_PROTECTION_STATE): text_sensor.text_sensor_schema(
        icon=ICON_ALERT_OUTLINE,
    ),
})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])

    await cg.register_component(var, config)
    await cg.register_parented(var, mhi)

    if CONF_PROTECTION_STATE in config:
        conf = config[CONF_PROTECTION_STATE]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(var.set_protection_state(sens))
