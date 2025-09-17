import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

CONF_VERTICAL = "vertical_vanes"
CONF_VERTICAL_SELECTS = [
    "Up",
    "Up/Center",
    "Center/Down",
    "Down",
    "Swing",
]
ICON_UP_DOWN = "mdi:arrow-up-down"
CONF_HORIZONTAL = "horizontal_vanes"
CONF_HORIZONTAL_SELECTS = [
    "Left",
    "Left/Center",
    "Center",
    "Center/Right",
    "Right",
    "Wide",
    "Spot",
    "Swing",
]
ICON_LEFT_RIGHT = "mdi:arrow-left-right"

CONF_FAN_SPEED = "fan_speed"
CONF_FAN_SPEED_SELECTS = [
    "Auto",
    "Quiet",
    "Low",
    "Medium",
    "High",
]
ICON_FAN = "mdi:fan"


mhi_ns = cg.esphome_ns.namespace('mhi')
MhiVerticalVanesSelect = mhi_ns.class_("MhiVerticalVanesSelect", select.Select, cg.Component)
MhiHorizontalVanesSelect = mhi_ns.class_("MhiHorizontalVanesSelect", select.Select, cg.Component)
MhiFanSpeedSelect = mhi_ns.class_("MhiFanSpeedSelect", select.Select, cg.Component)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    cv.Optional(CONF_VERTICAL): select.select_schema(
        MhiVerticalVanesSelect,
        icon=ICON_UP_DOWN
    ),
    cv.Optional(CONF_HORIZONTAL): select.select_schema(
        MhiHorizontalVanesSelect,
        icon=ICON_LEFT_RIGHT
    ),
    cv.Optional(CONF_FAN_SPEED): select.select_schema(
        MhiFanSpeedSelect,
        icon=ICON_FAN
    ),
}


async def to_code(config):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    if vertical_config := config.get(CONF_VERTICAL):
        sel = await select.new_select(
            vertical_config,
            options=CONF_VERTICAL_SELECTS,
        )
        await cg.register_parented(sel, mhi)
        await cg.register_component(sel, vertical_config)

    if horizontal_config := config.get(CONF_HORIZONTAL):
        sel = await select.new_select(
            horizontal_config,
            options=CONF_HORIZONTAL_SELECTS,
        )
        await cg.register_parented(sel, mhi)
        await cg.register_component(sel, horizontal_config)

    if fan_speed_config := config.get(CONF_FAN_SPEED):
        sel = await select.new_select(
            fan_speed_config,
            options=CONF_FAN_SPEED_SELECTS,
        )
        await cg.register_parented(sel, mhi)
        await cg.register_component(sel, fan_speed_config)
