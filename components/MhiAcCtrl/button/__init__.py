import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import MhiAcCtrl, CONF_MHI_AC_CTRL_ID

mhi_ns = cg.esphome_ns.namespace("mhi")
MhiSelfCleanButton = mhi_ns.class_("MhiSelfCleanButton", button.Button, cg.Component)

CONF_SELF_CLEAN = "self_clean"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
    cv.Optional(CONF_SELF_CLEAN): button.button_schema(
        MhiSelfCleanButton,
        icon="mdi:air-filter",
    ),
})

async def to_code(config):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    if self_clean_config := config.get(CONF_SELF_CLEAN):
        btn = await button.new_button(self_clean_config)
        await cg.register_component(btn, self_clean_config)
        await cg.register_parented(btn, mhi)
