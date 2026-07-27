import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import CONF_ID

from .driver_selection import (
    RX_DRIVERS,
    TX_DRIVERS,
    DriverSelectionError,
    resolve_tx_driver,
)

CONF_MHI_AC_CTRL_ID = "mhi_ac_ctrl_id"
CONF_FRAME_SIZE = "frame_size"
CONF_ROOM_TEMP_TIMEOUT = "room_temp_timeout"
CONF_VANES_UD = "initial_vertical_vanes_position"
CONF_VANES_LR = "initial_horizontal_vanes_position"
CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_RX_DRIVER = "rx_driver"
CONF_TX_DRIVER = "tx_driver"
CONF_FAN_PROFILE = "fan_profile"
CONF_FRAME_START_IDLE_MS = "frame_start_idle_ms"
CONF_RMT_SPI_FRAME_GAP_US = "rmt_spi_frame_gap_us"
CONF_TX_BACKGROUND_INTERVAL_MS = "tx_background_interval_ms"
CONF_RX_WORKER = "rx_worker"
CONF_RX_WORKER_START_DELAY_MS = "rx_worker_start_delay_ms"
CONF_RX_WORKER_STACK_SIZE = "rx_worker_stack_size"
CONF_RX_WORKER_PRIORITY = "rx_worker_priority"
CONF_RX_WORKER_CORE_ID = "rx_worker_core_id"
CONF_TX_WORKER = "tx_worker"
CONF_TX_WORKER_START_DELAY_MS = "tx_worker_start_delay_ms"
CONF_TX_WORKER_STACK_SIZE = "tx_worker_stack_size"
CONF_TX_WORKER_PRIORITY = "tx_worker_priority"
CONF_TX_WORKER_CORE_ID = "tx_worker_core_id"

DEFAULT_TX_BACKGROUND_INTERVAL_MS = 250
DEFAULT_WORKER_TX_BACKGROUND_INTERVAL_MS = 1000

CONF_VANES_POSITION = "position"
CONF_TEMPERATURE = "temperature"
CONF_EXTERNAL_TEMPERATURE_SENSOR = "external_temperature_sensor"

DEPENDENCIES = ["climate"]
AUTO_LOAD = ["binary_sensor", "select", "sensor", "switch", "text_sensor"]

mhi_ns = cg.esphome_ns.namespace("mhi_ac_ctrl")
MhiAcCtrl = mhi_ns.class_("MhiAcCtrl", cg.Component)


SetVerticalVanesAction = mhi_ns.class_("SetVerticalVanesAction", automation.Action)
SetHorizontalVanesAction = mhi_ns.class_("SetHorizontalVanesAction", automation.Action)
SetExternalRoomTemperatureAction = mhi_ns.class_("SetExternalRoomTemperatureAction", automation.Action)


def _validate_transport_configuration(config):
    explicit_tx_driver = config.get(CONF_TX_DRIVER)

    try:
        resolve_tx_driver(config[CONF_RX_DRIVER], explicit_tx_driver)
    except DriverSelectionError as err:
        raise cv.Invalid(str(err)) from err

    if config[CONF_RX_DRIVER] == "rmt_cs_spi" and (config[CONF_RX_WORKER] or config[CONF_TX_WORKER]):
        raise cv.Invalid("rx_driver: rmt_cs_spi is initially restricted to rx_worker: false and tx_worker: false")

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MhiAcCtrl),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_FRAME_SIZE, default=20): cv.one_of(20, 33, int=True),
            cv.Optional(CONF_ROOM_TEMP_TIMEOUT, default=60): cv.int_range(min=0, max=3600),
            cv.Optional(CONF_VANES_UD): cv.int_range(min=0, max=5),
            cv.Optional(CONF_VANES_LR): cv.int_range(min=0, max=8),
            cv.Optional(CONF_SCK_PIN): cv.int_,
            cv.Optional(CONF_MOSI_PIN): cv.int_,
            cv.Optional(CONF_MISO_PIN): cv.int_,
            cv.Optional(CONF_RX_DRIVER, default="fast_gpio_rx"): cv.one_of(*RX_DRIVERS, lower=True),
            cv.Optional(CONF_TX_DRIVER): cv.one_of(*TX_DRIVERS, lower=True),
            cv.Optional(CONF_FAN_PROFILE, default="four_speed"): cv.one_of("four_speed", "three_speed", lower=True),
            cv.Optional(CONF_FRAME_START_IDLE_MS, default=10): cv.int_range(min=1, max=50),
            cv.Optional(CONF_RMT_SPI_FRAME_GAP_US, default=1000): cv.int_range(min=500, max=5000),
            cv.Optional(CONF_TX_BACKGROUND_INTERVAL_MS): cv.int_range(min=0, max=60000),
            cv.Optional(CONF_RX_WORKER, default=False): cv.boolean,
            cv.Optional(CONF_RX_WORKER_START_DELAY_MS, default=5000): cv.int_range(min=0, max=30000),
            cv.Optional(CONF_RX_WORKER_STACK_SIZE, default=6144): cv.int_range(min=4096, max=16384),
            cv.Optional(CONF_RX_WORKER_PRIORITY, default=4): cv.int_range(min=1, max=10),
            cv.Optional(CONF_RX_WORKER_CORE_ID, default=0): cv.int_range(min=-1, max=1),
            cv.Optional(CONF_TX_WORKER, default=False): cv.boolean,
            cv.Optional(CONF_TX_WORKER_START_DELAY_MS, default=5000): cv.int_range(min=0, max=30000),
            cv.Optional(CONF_TX_WORKER_STACK_SIZE, default=6144): cv.int_range(min=4096, max=16384),
            cv.Optional(CONF_TX_WORKER_PRIORITY, default=4): cv.int_range(min=1, max=10),
            cv.Optional(CONF_TX_WORKER_CORE_ID, default=1): cv.int_range(min=-1, max=1),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_transport_configuration,
)


def _default_tx_background_interval_ms(config):
    if CONF_TX_BACKGROUND_INTERVAL_MS in config:
        return config[CONF_TX_BACKGROUND_INTERVAL_MS]

    if config[CONF_RX_WORKER] or config[CONF_TX_WORKER]:
        return DEFAULT_WORKER_TX_BACKGROUND_INTERVAL_MS

    return DEFAULT_TX_BACKGROUND_INTERVAL_MS


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_frame_size(config[CONF_FRAME_SIZE]))
    cg.add(var.set_room_temp_api_timeout(config[CONF_ROOM_TEMP_TIMEOUT]))
    effective_tx_driver = resolve_tx_driver(config[CONF_RX_DRIVER], config.get(CONF_TX_DRIVER))
    cg.add(var.set_rx_driver(config[CONF_RX_DRIVER]))
    cg.add(var.set_tx_driver(effective_tx_driver))
    cg.add(var.set_fan_profile(config[CONF_FAN_PROFILE]))
    cg.add(var.set_frame_start_idle_ms(config[CONF_FRAME_START_IDLE_MS]))
    cg.add(var.set_rmt_spi_frame_gap_us(config[CONF_RMT_SPI_FRAME_GAP_US]))
    cg.add(var.set_tx_background_interval_ms(_default_tx_background_interval_ms(config)))
    cg.add(var.set_rx_worker(config[CONF_RX_WORKER]))
    cg.add(var.set_rx_worker_start_delay_ms(config[CONF_RX_WORKER_START_DELAY_MS]))
    cg.add(var.set_rx_worker_stack_size(config[CONF_RX_WORKER_STACK_SIZE]))
    cg.add(var.set_rx_worker_priority(config[CONF_RX_WORKER_PRIORITY]))
    cg.add(var.set_rx_worker_core_id(config[CONF_RX_WORKER_CORE_ID]))
    cg.add(var.set_tx_worker(config[CONF_TX_WORKER]))
    cg.add(var.set_tx_worker_start_delay_ms(config[CONF_TX_WORKER_START_DELAY_MS]))
    cg.add(var.set_tx_worker_stack_size(config[CONF_TX_WORKER_STACK_SIZE]))
    cg.add(var.set_tx_worker_priority(config[CONF_TX_WORKER_PRIORITY]))
    cg.add(var.set_tx_worker_core_id(config[CONF_TX_WORKER_CORE_ID]))
    if CONF_EXTERNAL_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_EXTERNAL_TEMPERATURE_SENSOR])
        cg.add(var.set_external_room_temperature_sensor(sens))
    if CONF_VANES_UD in config:
        cg.add(var.set_vanes(config[CONF_VANES_UD]))
    if CONF_VANES_LR in config:
        cg.add(var.set_vanesLR(config[CONF_VANES_LR]))
    if CONF_SCK_PIN in config:
        cg.add(var.set_sck_pin(config[CONF_SCK_PIN]))
    if CONF_MOSI_PIN in config:
        cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
    if CONF_MISO_PIN in config:
        cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))


@automation.register_action(
    "climate.mhi.set_vertical_vanes",
    SetVerticalVanesAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
            cv.Required(CONF_VANES_POSITION): cv.templatable(cv.int_range(min=1, max=5)),
        }
    ),
    synchronous=True,
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
    synchronous=True,
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
    synchronous=True,
)
async def set_external_room_temperature_to_code(config, action_id, template_arg, args):
    mhi = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])
    var = cg.new_Pvariable(action_id, template_arg, mhi)
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, float)
    cg.add(var.set_temperature(template_))
    return var
