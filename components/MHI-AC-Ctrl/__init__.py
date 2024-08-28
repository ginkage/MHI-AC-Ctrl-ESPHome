import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

mhiacctrl = cg.esphome_ns.namespace('mhiacctrl')
MHIACCtrl = empty_component_ns.class_('MHI-AC-Ctrl', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MHI-Ac-Ctrl)
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
