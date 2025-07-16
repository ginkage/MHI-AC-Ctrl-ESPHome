import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import CONF_ID

CONF_MHI_AC_CTRL_ID = "mhi_ac_ctrl_id"
CONF_SCK_PIN = 'sck_pin'
CONF_MOSI_PIN = 'mosi_pin'
CONF_MISO_PIN = 'miso_pin'
CONF_FRAME_SIZE = 'frame_size'
CONF_ROOM_TEMP_TIMEOUT = 'room_temp_timeout'
CONF_VANES_UD = 'initial_vertical_vanes_position'
CONF_VANES_LR = 'initial_horizontal_vanes_position'

CONF_VANES_POSITION = 'position'
CONF_TEMPERATURE = 'temperature'
CONF_EXTERNAL_TEMPERATURE_SENSOR = 'external_temperature_sensor'

mhi_ns = cg.esphome_ns.namespace('mhi')
MhiAcCtrl = mhi_ns.class_('MhiPlatform', cg.Component)

SetVerticalVanesAction = mhi_ns.class_("SetVerticalVanesAction", automation.Action)
SetHorizontalVanesAction = mhi_ns.class_("SetHorizontalVanesAction", automation.Action)
SetExternalRoomTemperatureAction = mhi_ns.class_("SetExternalRoomTemperatureAction", automation.Action)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MhiAcCtrl),
    cv.Optional(CONF_SCK_PIN, default = 14):  cv.uint16_t,
    cv.Optional(CONF_MOSI_PIN, default = 13): cv.uint16_t,
    cv.Optional(CONF_MISO_PIN, default = 12): cv.uint16_t,
    cv.Optional(CONF_EXTERNAL_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_FRAME_SIZE, default=20): cv.int_range(min=20, max=33),
    cv.Optional(CONF_ROOM_TEMP_TIMEOUT, default=60): cv.int_range(min=0, max=3600),
    cv.Optional(CONF_VANES_UD): cv.int_range(min=0, max=5),
    cv.Optional(CONF_VANES_LR): cv.int_range(min=0, max=8),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID],
                           config[CONF_SCK_PIN],
                           config[CONF_MOSI_PIN],
                           config[CONF_MISO_PIN])
    await cg.register_component(var, config)
    cg.add(var.set_frame_size(config[CONF_FRAME_SIZE]))
    cg.add(var.set_room_temp_api_timeout(config[CONF_ROOM_TEMP_TIMEOUT]))
    if CONF_EXTERNAL_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_EXTERNAL_TEMPERATURE_SENSOR])
        cg.add(var.set_external_room_temperature_sensor(sens))
    if CONF_VANES_UD in config:
        cg.add(var.set_vanes(config[CONF_VANES_UD]))
    if CONF_VANES_LR in config:
        cg.add(var.set_vanesLR(config[CONF_VANES_LR]))


@automation.register_action(
    "climate.mhi.set_vertical_vanes",
    SetVerticalVanesAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_VANES_POSITION): cv.templatable(cv.int_range(min=1, max=5)),
        }
    ),
)
async def set_vertical_vanes_to_code(config, action_id, template_arg, args):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    var = cg.new_Pvariable(action_id, template_arg, mhi)
    template_ = await cg.templatable(config[CONF_VANES_POSITION], args, int)
    cg.add(var.set_position(template_))
    return var

@automation.register_action(
    "climate.mhi.set_horizontal_vanes",
    SetHorizontalVanesAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_VANES_POSITION): cv.templatable(cv.int_range(min=1, max=8)),
        }
    ),
)
async def set_horizontal_vanes_to_code(config, action_id, template_arg, args):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    var = cg.new_Pvariable(action_id, template_arg, mhi)
    template_ = await cg.templatable(config[CONF_VANES_POSITION], args, int)
    cg.add(var.set_position(template_))
    return var


@automation.register_action(
    "climate.mhi.set_external_room_temperature",
    SetExternalRoomTemperatureAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_TEMPERATURE): cv.templatable(cv.float_),
        }
    ),
)
async def set_external_room_temperature_to_code(config, action_id, template_arg, args):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    var = cg.new_Pvariable(action_id, template_arg, mhi)
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, float)
    cg.add(var.set_temperature(template_))
    return var
