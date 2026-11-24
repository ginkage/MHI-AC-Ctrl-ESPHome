import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_HOUR,
    UNIT_KILOWATT_HOURS,
    UNIT_SECOND,
    UNIT_WATT,
)

from .. import CONF_MHI_AC_CTRL_ID, MhiAcCtrl, mhi_ns

MhiSensors = mhi_ns.class_("MhiSensors", cg.Component)

CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_TARGET_TEMPERATURE = "target_temperature"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_RETURN_AIR_TEMPERATURE = "return_air_temperature"
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_CURRENT_POWER = "current_power"
CONF_INDOOR_UNIT_FAN_SPEED = "indoor_unit_fan_speed"
CONF_OUTDOOR_UNIT_FAN_SPEED = "outdoor_unit_fan_speed"
CONF_INDOOR_UNIT_TOTAL_RUN_TIME = "indoor_unit_total_run_time"
CONF_COMPRESSOR_TOTAL_RUN_TIME = "compressor_total_run_time"
CONF_ENERGY_USED = "energy_used"
CONF_ESTIMATED_POWER = "estimated_power"
CONF_ESTIMATED_ENERGY = "estimated_energy"
CONF_INDOOR_UNIT_THI_R1 = "indoor_unit_thi_r1"
CONF_INDOOR_UNIT_THI_R2 = "indoor_unit_thi_r2"
CONF_INDOOR_UNIT_THI_R3 = "indoor_unit_thi_r3"
CONF_OUTDOOR_UNIT_THO_R1 = "outdoor_unit_tho_r1"
CONF_OUTDOOR_UNIT_EXPANSION_VALVE = "outdoor_unit_expansion_valve"
CONF_OUTDOOR_UNIT_DISCHARGE_PIPE = "outdoor_unit_discharge_pipe"
CONF_OUTDOOR_UNIT_DISCHARGE_PIPE_SUPER_HEAT = "outdoor_unit_discharge_pipe_super_heat"
CONF_PROTECTION_STATE_NUMBER = "protection_state_number"
CONF_OPDATA_OLDEST_AGE = "opdata_oldest_age"
CONF_OPDATA_STALE_COUNT = "opdata_stale_count"
CONF_OPDATA_TIMEOUT_EVENTS = "opdata_timeout_events"

ICON_SINE = "mdi:sine-wave"
ICON_CURRENT = "mdi:current-ac"
ICON_CLOCK = "mdi:clock"
ICON_LIGHTNING_BOLT = "mdi:lightning-bolt"
ICON_VALVE = "mdi:valve"
ICON_ALERT_OUTLINE = "mdi:shield-alert-outline"
ICON_DATABASE_CLOCK = "mdi:database-clock"
ICON_DATABASE_ALERT = "mdi:database-alert"
ICON_TIMER_ALERT = "mdi:timer-alert-outline"
UNIT_PULSE = "pulse"

MHI_OPDATA_REQ_RETURN_AIR = 1 << 2
MHI_OPDATA_REQ_THI_R1 = 1 << 3
MHI_OPDATA_REQ_THI_R2 = 1 << 4
MHI_OPDATA_REQ_THI_R3 = 1 << 5
MHI_OPDATA_REQ_IU_FANSPEED = 1 << 6
MHI_OPDATA_REQ_TOTAL_IU_RUN = 1 << 7
MHI_OPDATA_REQ_OUTDOOR = 1 << 8
MHI_OPDATA_REQ_THO_R1 = 1 << 9
MHI_OPDATA_REQ_COMP = 1 << 10
MHI_OPDATA_REQ_TD = 1 << 11
MHI_OPDATA_REQ_CT = 1 << 12
MHI_OPDATA_REQ_TDSH = 1 << 13
MHI_OPDATA_REQ_PROTECTION_NO = 1 << 14
MHI_OPDATA_REQ_OU_FANSPEED = 1 << 15
MHI_OPDATA_REQ_TOTAL_COMP_RUN = 1 << 17
MHI_OPDATA_REQ_OU_EEV1 = 1 << 18
MHI_OPDATA_REQ_KWH = 1 << 19

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MhiSensors),
        cv.GenerateID(CONF_MHI_AC_CTRL_ID): cv.use_id(MhiAcCtrl),
        cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RETURN_AIR_TEMPERATURE): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COMPRESSOR_FREQUENCY): sensor.sensor_schema(
            icon=ICON_SINE,
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT_POWER): sensor.sensor_schema(
            icon=ICON_CURRENT,
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INDOOR_UNIT_FAN_SPEED): sensor.sensor_schema(
            icon=ICON_FAN,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_UNIT_FAN_SPEED): sensor.sensor_schema(
            icon=ICON_FAN,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INDOOR_UNIT_TOTAL_RUN_TIME): sensor.sensor_schema(
            icon=ICON_CLOCK,
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_COMPRESSOR_TOTAL_RUN_TIME): sensor.sensor_schema(
            icon=ICON_CLOCK,
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_ENERGY_USED): sensor.sensor_schema(
            icon=ICON_LIGHTNING_BOLT,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_ESTIMATED_POWER): sensor.sensor_schema(
            icon=ICON_LIGHTNING_BOLT,
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ESTIMATED_ENERGY): sensor.sensor_schema(
            icon=ICON_LIGHTNING_BOLT,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_INDOOR_UNIT_THI_R1): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INDOOR_UNIT_THI_R2): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_INDOOR_UNIT_THI_R3): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_UNIT_THO_R1): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_UNIT_EXPANSION_VALVE): sensor.sensor_schema(
            icon=ICON_VALVE,
            unit_of_measurement=UNIT_PULSE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_UNIT_DISCHARGE_PIPE): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_UNIT_DISCHARGE_PIPE_SUPER_HEAT): sensor.sensor_schema(
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PROTECTION_STATE_NUMBER): sensor.sensor_schema(
            icon=ICON_ALERT_OUTLINE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OPDATA_OLDEST_AGE): sensor.sensor_schema(
            icon=ICON_DATABASE_CLOCK,
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_OPDATA_STALE_COUNT): sensor.sensor_schema(
            icon=ICON_DATABASE_ALERT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_OPDATA_TIMEOUT_EVENTS): sensor.sensor_schema(
            icon=ICON_TIMER_ALERT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_MHI_AC_CTRL_ID])

    await cg.register_component(var, config)
    await cg.register_parented(var, parent)

    opdata_mask = 0

    if CONF_ROOM_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_ROOM_TEMPERATURE])
        cg.add(var.set_room_temperature(sens))
        cg.add(parent.set_room_temp_sensor(sens))

    if CONF_TARGET_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TARGET_TEMPERATURE])
        cg.add(var.set_target_temperature(sens))
        cg.add(parent.set_target_temp_sensor(sens))

    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature(sens))
        cg.add(parent.set_outdoor_temp_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_OUTDOOR

    if CONF_RETURN_AIR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_RETURN_AIR_TEMPERATURE])
        cg.add(var.set_return_air_temperature(sens))
        cg.add(parent.set_return_air_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_RETURN_AIR

    if CONF_COMPRESSOR_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_COMPRESSOR_FREQUENCY])
        cg.add(var.set_compressor_frequency(sens))
        cg.add(parent.set_compressor_frequency_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_COMP

    if CONF_CURRENT_POWER in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_POWER])
        cg.add(var.set_current_power(sens))
        cg.add(parent.set_current_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_CT

    if CONF_INDOOR_UNIT_FAN_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_UNIT_FAN_SPEED])
        cg.add(var.set_indoor_unit_fan_speed(sens))
        cg.add(parent.set_indoor_unit_fan_speed_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_IU_FANSPEED

    if CONF_OUTDOOR_UNIT_FAN_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_UNIT_FAN_SPEED])
        cg.add(var.set_outdoor_unit_fan_speed(sens))
        cg.add(parent.set_outdoor_unit_fan_speed_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_OU_FANSPEED

    if CONF_INDOOR_UNIT_TOTAL_RUN_TIME in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_UNIT_TOTAL_RUN_TIME])
        cg.add(var.set_indoor_unit_total_run_time(sens))
        cg.add(parent.set_indoor_unit_total_run_time_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_TOTAL_IU_RUN

    if CONF_COMPRESSOR_TOTAL_RUN_TIME in config:
        sens = await sensor.new_sensor(config[CONF_COMPRESSOR_TOTAL_RUN_TIME])
        cg.add(var.set_compressor_total_run_time(sens))
        cg.add(parent.set_compressor_total_run_time_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_TOTAL_COMP_RUN

    if CONF_ENERGY_USED in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY_USED])
        cg.add(var.set_energy_used(sens))
        cg.add(parent.set_energy_used_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_KWH

    if CONF_ESTIMATED_POWER in config:
        sens = await sensor.new_sensor(config[CONF_ESTIMATED_POWER])
        cg.add(var.set_estimated_power(sens))
        cg.add(parent.set_estimated_power_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_CT

    if CONF_ESTIMATED_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_ESTIMATED_ENERGY])
        cg.add(var.set_estimated_energy(sens))
        cg.add(parent.set_estimated_energy_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_CT

    if CONF_INDOOR_UNIT_THI_R1 in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_UNIT_THI_R1])
        cg.add(var.set_indoor_unit_thi_r1(sens))
        cg.add(parent.set_indoor_unit_thi_r1_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_THI_R1

    if CONF_INDOOR_UNIT_THI_R2 in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_UNIT_THI_R2])
        cg.add(var.set_indoor_unit_thi_r2(sens))
        cg.add(parent.set_indoor_unit_thi_r2_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_THI_R2

    if CONF_INDOOR_UNIT_THI_R3 in config:
        sens = await sensor.new_sensor(config[CONF_INDOOR_UNIT_THI_R3])
        cg.add(var.set_indoor_unit_thi_r3(sens))
        cg.add(parent.set_indoor_unit_thi_r3_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_THI_R3

    if CONF_OUTDOOR_UNIT_THO_R1 in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_UNIT_THO_R1])
        cg.add(var.set_outdoor_unit_tho_r1(sens))
        cg.add(parent.set_outdoor_unit_tho_r1_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_THO_R1

    if CONF_OUTDOOR_UNIT_EXPANSION_VALVE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_UNIT_EXPANSION_VALVE])
        cg.add(var.set_outdoor_unit_expansion_valve(sens))
        cg.add(parent.set_outdoor_unit_expansion_valve_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_OU_EEV1

    if CONF_OUTDOOR_UNIT_DISCHARGE_PIPE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_UNIT_DISCHARGE_PIPE])
        cg.add(var.set_outdoor_unit_discharge_pipe(sens))
        cg.add(parent.set_outdoor_unit_discharge_pipe_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_TD

    if CONF_OUTDOOR_UNIT_DISCHARGE_PIPE_SUPER_HEAT in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_UNIT_DISCHARGE_PIPE_SUPER_HEAT])
        cg.add(var.set_outdoor_unit_discharge_pipe_super_heat(sens))
        cg.add(parent.set_outdoor_unit_discharge_pipe_super_heat_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_TDSH

    if CONF_PROTECTION_STATE_NUMBER in config:
        sens = await sensor.new_sensor(config[CONF_PROTECTION_STATE_NUMBER])
        cg.add(var.set_protection_state_number(sens))
        cg.add(parent.set_protection_state_number_sensor(sens))
        opdata_mask |= MHI_OPDATA_REQ_PROTECTION_NO

    if CONF_OPDATA_OLDEST_AGE in config:
        sens = await sensor.new_sensor(config[CONF_OPDATA_OLDEST_AGE])
        cg.add(var.set_opdata_oldest_age(sens))
        cg.add(parent.set_opdata_oldest_age_sensor(sens))

    if CONF_OPDATA_STALE_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_OPDATA_STALE_COUNT])
        cg.add(var.set_opdata_stale_count(sens))
        cg.add(parent.set_opdata_stale_count_sensor(sens))

    if CONF_OPDATA_TIMEOUT_EVENTS in config:
        sens = await sensor.new_sensor(config[CONF_OPDATA_TIMEOUT_EVENTS])
        cg.add(var.set_opdata_timeout_events(sens))
        cg.add(parent.set_opdata_timeout_events_sensor(sens))

    if opdata_mask != 0:
        cg.add(parent.add_opdata_mask(opdata_mask))
