# 1. Stelle (ca. Zeile 67)
@automation.register_action(
    "climate.mhi.set_vertical_vanes",
    SetVerticalVanesAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_VANES_POSITION): cv.templatable(cv.int_range(min=1, max=5)),
        }
    ),
    synchronous=False, # HIER stand vorher das [span_4] - bitte löschen!
)

# 2. Stelle
@automation.register_action(
    "climate.mhi.set_horizontal_vanes",
    SetHorizontalVanesAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_VANES_POSITION): cv.templatable(cv.int_range(min=1, max=8)),
        }
    ),
    synchronous=False, 
)

# 3. Stelle
@automation.register_action(
    "climate.mhi.set_external_room_temperature",
    SetExternalRoomTemperatureAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_TEMPERATURE): cv.templatable(cv.float_),
        }
    ),
    synchronous=False,
)
