import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.components.esp32 import get_esp32_variant, include_builtin_idf_component
from esphome.const import CONF_ID
from esphome.core import CORE

from .driver_selection import (
    RX_DRIVERS,
    TX_DRIVERS,
    DriverSelectionError,
    resolve_tx_driver,
)
from .mhi_transport_codegen import (
    MhiTransportBuildInputs,
    build_internal_transport_schema,
)
from .mhi_transport_registry import (
    TransportConfigurationError,
    build_selected_transports,
    build_transport_schemas,
    resolve_selected_compile_defines,
    resolve_selected_idf_components,
    resolve_transport_tuning,
    validate_driver_subsections,
    validate_selected_transport_target,
)

CONF_MHI_AC_CTRL_ID = "mhi_ac_ctrl_id"
CONF_FRAME_SIZE = "frame_size"
CONF_ROOM_TEMP_TIMEOUT = "room_temp_timeout"
CONF_ROOM_TEMPERATURE_PUBLISH_INTERVAL = "room_temperature_publish_interval"
CONF_ROOM_TEMPERATURE_IMMEDIATE_DELTA = "room_temperature_immediate_delta"
CONF_OPDATA_FRESHNESS_TIMEOUT = "opdata_freshness_timeout"
CONF_OPDATA_POLL_INTERVALS = "opdata_poll_intervals"

# Bit positions match MhiOpdataRequestMask in mhi_tx_builder.h.
OPDATA_POLL_REQUESTS = {
    "mode": 1 << 0,
    "target_temperature": 1 << 1,
    "return_air_temperature": 1 << 2,
    "indoor_unit_thi_r1": 1 << 3,
    "indoor_unit_thi_r2": 1 << 4,
    "indoor_unit_thi_r3": 1 << 5,
    "indoor_unit_fan_speed": 1 << 6,
    "indoor_unit_total_run_time": 1 << 7,
    "outdoor_temperature": 1 << 8,
    "outdoor_unit_tho_r1": 1 << 9,
    "compressor_frequency": 1 << 10,
    "outdoor_unit_discharge_pipe": 1 << 11,
    "current_power": 1 << 12,
    "outdoor_unit_discharge_pipe_super_heat": 1 << 13,
    "protection_state_number": 1 << 14,
    "outdoor_unit_fan_speed": 1 << 15,
    "defrost": 1 << 16,
    "compressor_total_run_time": 1 << 17,
    "outdoor_unit_expansion_valve": 1 << 18,
    "energy_used": 1 << 19,
    "silent_mode": 1 << 20,
}


def _poll_interval_at_least_one_second(value):
    interval = cv.positive_time_period_milliseconds(value)
    if interval.total_milliseconds < 1000:
        raise cv.Invalid("OPDATA polling intervals must be at least 1s")
    if interval.total_milliseconds > 86400000:
        raise cv.Invalid("OPDATA polling intervals must be no greater than 24h")
    return interval


CONF_POWER_ESTIMATION = "power_estimation"
CONF_NOMINAL_VOLTAGE = "nominal_voltage"
CONF_VOLTAGE_SENSOR = "voltage_sensor"
CONF_VOLTAGE_TIMEOUT = "voltage_timeout"
CONF_POWER_FACTOR = "power_factor"
CONF_POWER_FACTOR_SENSOR = "power_factor_sensor"
CONF_POWER_FACTOR_TIMEOUT = "power_factor_timeout"
CONF_STANDBY_POWER = "standby_power"
CONF_MAX_SAMPLE_INTERVAL = "max_sample_interval"
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
CONF_COMMAND_CONFIRMATION_TIMEOUT_MS = "command_confirmation_timeout_ms"
CONF_COMMAND_FINAL_CONFIRMATION_GRACE_MS = "command_final_confirmation_grace_ms"
CONF_COMMAND_WORKER = "command_worker"
CONF_COMMAND_WORKER_START_DELAY_MS = "command_worker_start_delay_ms"
CONF_COMMAND_WORKER_STACK_SIZE = "command_worker_stack_size"
CONF_COMMAND_WORKER_PRIORITY = "command_worker_priority"
CONF_COMMAND_WORKER_CORE_ID = "command_worker_core_id"

DEFAULT_TX_BACKGROUND_INTERVAL_MS = 250
DEFAULT_COMMAND_CONFIRMATION_TIMEOUT_MS = 1500
DEFAULT_COMMAND_FINAL_CONFIRMATION_GRACE_MS = 1000

CONF_VANES_POSITION = "position"
CONF_TEMPERATURE = "temperature"
CONF_EXTERNAL_TEMPERATURE_SENSOR = "external_temperature_sensor"
CONF_EXTERNAL_TEMPERATURE_FALLBACK_ON_HOMEASSISTANT_DISCONNECT = (
    "external_temperature_fallback_on_homeassistant_disconnect"
)
CONF_EXTERNAL_TEMPERATURE_DISCONNECT_GRACE_PERIOD = "external_temperature_disconnect_grace_period"

DEPENDENCIES = ["climate"]
AUTO_LOAD = ["binary_sensor", "select", "sensor", "switch", "text_sensor"]

mhi_ns = cg.esphome_ns.namespace("mhi_ac_ctrl")
MhiAcCtrl = mhi_ns.class_("MhiAcCtrl", cg.Component)

SetVerticalVanesAction = mhi_ns.class_("SetVerticalVanesAction", automation.Action)
SetHorizontalVanesAction = mhi_ns.class_("SetHorizontalVanesAction", automation.Action)
SetExternalRoomTemperatureAction = mhi_ns.class_("SetExternalRoomTemperatureAction", automation.Action)

TRANSPORT_SCHEMAS = build_transport_schemas()


def _validate_transport_configuration(config):
    explicit_tx_driver = config.get(CONF_TX_DRIVER)
    try:
        resolve_tx_driver(config[CONF_RX_DRIVER], explicit_tx_driver)
        validate_driver_subsections(config)
        validate_selected_transport_target(
            config,
            platform=CORE.target_platform,
            framework=CORE.target_framework,
            variant=get_esp32_variant() if CORE.is_esp32 else None,
        )
    except (DriverSelectionError, TransportConfigurationError) as err:
        raise cv.Invalid(str(err)) from err

    return config


def _validate_external_temperature_configuration(config):
    if (
        config[CONF_EXTERNAL_TEMPERATURE_FALLBACK_ON_HOMEASSISTANT_DISCONNECT]
        and CONF_EXTERNAL_TEMPERATURE_SENSOR not in config
    ):
        raise cv.Invalid(
            f"{CONF_EXTERNAL_TEMPERATURE_FALLBACK_ON_HOMEASSISTANT_DISCONNECT} requires "
            f"{CONF_EXTERNAL_TEMPERATURE_SENSOR}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MhiAcCtrl),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE_FALLBACK_ON_HOMEASSISTANT_DISCONNECT, default=False): cv.boolean,
            cv.Optional(
                CONF_EXTERNAL_TEMPERATURE_DISCONNECT_GRACE_PERIOD, default="60s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FRAME_SIZE, default=20): cv.one_of(20, 33, int=True),
            cv.Optional(CONF_ROOM_TEMP_TIMEOUT, default=60): cv.int_range(min=0, max=3600),
            cv.Optional(CONF_ROOM_TEMPERATURE_PUBLISH_INTERVAL, default="15s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ROOM_TEMPERATURE_IMMEDIATE_DELTA, default=1.0): cv.float_range(min=0.0, max=10.0),
            cv.Optional(CONF_OPDATA_FRESHNESS_TIMEOUT, default="120s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OPDATA_POLL_INTERVALS): cv.Schema(
                {cv.Optional(name): _poll_interval_at_least_one_second for name in OPDATA_POLL_REQUESTS}
            ),
            cv.Optional(CONF_POWER_ESTIMATION): cv.Schema(
                {
                    cv.Optional(CONF_NOMINAL_VOLTAGE, default=230.0): cv.float_range(min=1.0, max=500.0),
                    cv.Optional(CONF_VOLTAGE_SENSOR): cv.use_id(sensor.Sensor),
                    cv.Optional(CONF_VOLTAGE_TIMEOUT, default="5min"): cv.positive_time_period_milliseconds,
                    cv.Optional(CONF_POWER_FACTOR, default=1.0): cv.float_range(min=0.1, max=1.0),
                    cv.Optional(CONF_POWER_FACTOR_SENSOR): cv.use_id(sensor.Sensor),
                    cv.Optional(CONF_POWER_FACTOR_TIMEOUT, default="5min"): cv.positive_time_period_milliseconds,
                    cv.Optional(CONF_STANDBY_POWER, default=0.0): cv.float_range(min=0.0, max=500.0),
                    cv.Optional(CONF_MAX_SAMPLE_INTERVAL, default="5min"): cv.positive_time_period_milliseconds,
                }
            ),
            cv.Optional(CONF_VANES_UD): cv.int_range(min=0, max=5),
            cv.Optional(CONF_VANES_LR): cv.int_range(min=0, max=8),
            cv.Optional(CONF_SCK_PIN): cv.int_,
            cv.Optional(CONF_MOSI_PIN): cv.int_,
            cv.Optional(CONF_MISO_PIN): cv.int_,
            cv.Optional(CONF_RX_DRIVER, default="fast_gpio_rx"): cv.one_of(*RX_DRIVERS, lower=True),
            cv.Optional(CONF_TX_DRIVER): cv.one_of(*TX_DRIVERS, lower=True),
            cv.Optional(CONF_FAN_PROFILE, default="four_speed"): cv.one_of("four_speed", "three_speed", lower=True),
            cv.Optional(CONF_FRAME_START_IDLE_MS): cv.int_range(min=1, max=50),
            cv.Optional(CONF_RMT_SPI_FRAME_GAP_US): cv.int_range(min=500, max=5000),
            **{cv.Optional(name): schema for name, schema in TRANSPORT_SCHEMAS.items()},
            **build_internal_transport_schema(),
            cv.Optional(CONF_TX_BACKGROUND_INTERVAL_MS): cv.int_range(min=0, max=60000),
            cv.Optional(
                CONF_COMMAND_CONFIRMATION_TIMEOUT_MS, default=DEFAULT_COMMAND_CONFIRMATION_TIMEOUT_MS
            ): cv.int_range(min=100, max=60000),
            cv.Optional(
                CONF_COMMAND_FINAL_CONFIRMATION_GRACE_MS, default=DEFAULT_COMMAND_FINAL_CONFIRMATION_GRACE_MS
            ): cv.int_range(min=0, max=60000),
            cv.Optional(CONF_COMMAND_WORKER, default=False): cv.boolean,
            cv.Optional(CONF_COMMAND_WORKER_START_DELAY_MS, default=0): cv.int_range(min=0, max=30000),
            cv.Optional(CONF_COMMAND_WORKER_STACK_SIZE, default=6144): cv.int_range(min=4096, max=16384),
            cv.Optional(CONF_COMMAND_WORKER_PRIORITY, default=4): cv.int_range(min=1, max=10),
            cv.Optional(CONF_COMMAND_WORKER_CORE_ID, default=-1): cv.int_range(min=-1, max=1),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_transport_configuration,
    _validate_external_temperature_configuration,
)


def _default_tx_background_interval_ms(config):
    return config.get(CONF_TX_BACKGROUND_INTERVAL_MS, DEFAULT_TX_BACKGROUND_INTERVAL_MS)


async def to_code(config):
    for define in resolve_selected_compile_defines(config):
        cg.add_define(define)

    for component in resolve_selected_idf_components(config):
        include_builtin_idf_component(component)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_frame_size(config[CONF_FRAME_SIZE]))
    cg.add(var.set_room_temp_api_timeout(config[CONF_ROOM_TEMP_TIMEOUT]))
    cg.add(var.set_room_temperature_publish_interval_ms(config[CONF_ROOM_TEMPERATURE_PUBLISH_INTERVAL]))
    cg.add(var.set_room_temperature_immediate_delta(config[CONF_ROOM_TEMPERATURE_IMMEDIATE_DELTA]))
    cg.add(var.set_opdata_freshness_timeout_ms(config[CONF_OPDATA_FRESHNESS_TIMEOUT]))
    for request_name, interval in config.get(CONF_OPDATA_POLL_INTERVALS, {}).items():
        cg.add(var.set_opdata_poll_interval_ms(OPDATA_POLL_REQUESTS[request_name], interval))
    if CONF_POWER_ESTIMATION in config:
        estimation = config[CONF_POWER_ESTIMATION]
        cg.add(
            var.configure_power_estimation(
                estimation[CONF_NOMINAL_VOLTAGE],
                estimation[CONF_POWER_FACTOR],
                estimation[CONF_STANDBY_POWER],
                estimation[CONF_MAX_SAMPLE_INTERVAL],
            )
        )
        if CONF_VOLTAGE_SENSOR in estimation:
            voltage_sensor = await cg.get_variable(estimation[CONF_VOLTAGE_SENSOR])
            cg.add(var.set_external_voltage_sensor(voltage_sensor))
            cg.add(var.set_external_voltage_timeout_ms(estimation[CONF_VOLTAGE_TIMEOUT]))
        if CONF_POWER_FACTOR_SENSOR in estimation:
            power_factor_sensor = await cg.get_variable(estimation[CONF_POWER_FACTOR_SENSOR])
            cg.add(var.set_external_power_factor_sensor(power_factor_sensor))
            cg.add(var.set_external_power_factor_timeout_ms(estimation[CONF_POWER_FACTOR_TIMEOUT]))
    effective_tx_driver = resolve_tx_driver(config[CONF_RX_DRIVER], config.get(CONF_TX_DRIVER))
    transport_tuning = resolve_transport_tuning(config)
    transport_inputs = MhiTransportBuildInputs(
        frame_size=config[CONF_FRAME_SIZE],
        sck_pin=config.get(CONF_SCK_PIN, -1),
        mosi_pin=config.get(CONF_MOSI_PIN, -1),
        miso_pin=config.get(CONF_MISO_PIN, -1),
        frame_start_idle_ms=transport_tuning.frame_start_idle_ms,
        rmt_spi_frame_gap_us=transport_tuning.rmt_spi_frame_gap_us,
        tx_driver=effective_tx_driver,
    )
    primary_transport, recovery_transport = await build_selected_transports(config, transport_inputs)
    cg.add(var.set_primary_transport(primary_transport))
    if recovery_transport is not None:
        cg.add(var.set_recovery_transport(recovery_transport))

    cg.add(var.set_fan_profile(config[CONF_FAN_PROFILE]))
    cg.add(var.set_tx_background_interval_ms(_default_tx_background_interval_ms(config)))
    cg.add(var.set_command_confirmation_timeout_ms(config[CONF_COMMAND_CONFIRMATION_TIMEOUT_MS]))
    cg.add(var.set_command_final_confirmation_grace_ms(config[CONF_COMMAND_FINAL_CONFIRMATION_GRACE_MS]))
    cg.add(var.set_command_worker(config[CONF_COMMAND_WORKER]))
    cg.add(var.set_command_worker_start_delay_ms(config[CONF_COMMAND_WORKER_START_DELAY_MS]))
    cg.add(var.set_command_worker_stack_size(config[CONF_COMMAND_WORKER_STACK_SIZE]))
    cg.add(var.set_command_worker_priority(config[CONF_COMMAND_WORKER_PRIORITY]))
    cg.add(var.set_command_worker_core_id(config[CONF_COMMAND_WORKER_CORE_ID]))
    if CONF_EXTERNAL_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_EXTERNAL_TEMPERATURE_SENSOR])
        cg.add(var.set_external_room_temperature_sensor(sens))
    cg.add(
        var.set_external_temperature_fallback_on_homeassistant_disconnect(
            config[CONF_EXTERNAL_TEMPERATURE_FALLBACK_ON_HOMEASSISTANT_DISCONNECT]
        )
    )
    cg.add(
        var.set_external_temperature_disconnect_grace_period_ms(
            config[CONF_EXTERNAL_TEMPERATURE_DISCONNECT_GRACE_PERIOD]
        )
    )
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
