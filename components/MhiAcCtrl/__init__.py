import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import climate

AUTO_LOAD = ["sensor", "climate", "text_sensor", "binary_sensor"]
CONF_FRAME_SIZE = 'frame_size'
CONF_ROOM_TEMP_TIMEOUT = 'room_temp_timeout'
CONF_VANES_UD = 'vanes_ud'
CONF_VANES_LR = 'vanes_lr'

mhiacctrl = cg.esphome_ns.namespace('mhiacctrl')
MhiAcCtrl = cg.global_ns.class_('MhiAcCtrl', cg.Component, climate.Climate)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MhiAcCtrl),
    cv.Optional(CONF_FRAME_SIZE, default=20): cv.int_range(min=20, max=33),
    cv.Optional(CONF_ROOM_TEMP_TIMEOUT, default=60): cv.int_range(min=0, max=3600),
    cv.Optional(CONF_VANES_UD, default=0): cv.int_range(min=0, max=5),
    cv.Optional(CONF_VANES_LR, default=0): cv.int_range(min=0, max=8),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_frame_size(config[CONF_FRAME_SIZE]))
    cg.add(var.set_room_temp_api_timeout(config[CONF_ROOM_TEMP_TIMEOUT]))
    cg.add(var.set_vanes_ud(config[CONF_VANES_UD]))
    cg.add(var.set_vanes_lr(config[CONF_VANES_LR]))