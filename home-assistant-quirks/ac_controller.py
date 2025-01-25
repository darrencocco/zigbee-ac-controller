from typing import Final, Any

import zigpy.types as t
from zigpy.quirks import CustomCluster
from zigpy.quirks.v2 import QuirkBuilder, EntityType, ReportingConfig
from zigpy.zcl import BaseAttributeDefs, foundation
from zigpy.zcl.foundation import ZCLAttributeDef

class Operation_Modes(t.enum16):
    Off: t.uint16_t = 0x0000
    Auto: t.uint16_t = 0x0001
    Fan: t.uint16_t = 0x0002
    Dehumidify: t.uint16_t = 0x0003
    Cool: t.uint16_t = 0x0004
    Heat: t.uint16_t = 0x0005

class Fan_Modes(t.enum16):
    Slow: t.uint16_t = 0x0000
    Medium: t.uint16_t = 0x0001
    Fast: t.uint16_t = 0x0002
    Ludicrous: t.uint16_t = 0x0003
    Turbo_Ludicrous: t.uint16_t = 0x0004

class AzbBinaryOutput(CustomCluster):
    """Esp binary output."""

    cluster_id: t.uint16_t = 0xFF10
    ep_attribute = "binary_output"

    @property
    def _is_manuf_specific(self) -> bool:
        """Return True if cluster_id is within manufacturer specific range."""
        return False

    class AttributeDefs(BaseAttributeDefs):
        active_text: Final = ZCLAttributeDef(
            id=0x0004, type=t.CharacterString, access="r*w"
        )
        description: Final = ZCLAttributeDef(
            id=0x001C, type=t.CharacterString, access="r*w"
        )
        inactive_text: Final = ZCLAttributeDef(
            id=0x002E, type=t.CharacterString, access="r*w"
        )
        minimum_off_time: Final = ZCLAttributeDef(
            id=0x0042, type=t.uint32_t, access="r*w"
        )
        minimum_on_time: Final = ZCLAttributeDef(
            id=0x0043, type=t.uint32_t, access="r*w"
        )
        out_of_service: Final = ZCLAttributeDef(
            id=0x0051, type=t.Bool, access="r*w", mandatory=True
        )
        polarity: Final = ZCLAttributeDef(id=0x0054, type=t.enum8, access="r")
        present_value: Final = ZCLAttributeDef(
            id=0x0055, type=t.Bool, access="rwp", mandatory=True
        )
        # 0x0057: ('priority_array', TODO.array),  # Array of 16 structures of (boolean,
        # single precision)
        reliability: Final = ZCLAttributeDef(id=0x0067, type=t.enum8, access="r*w")
        relinquish_default: Final = ZCLAttributeDef(
            id=0x0068, type=t.Bool, access="r*w"
        )
        resolution: Final = ZCLAttributeDef(
            id=0x006A, type=t.Single, access="r"
        )  # Does not seem to be in binary_output
        status_flags: Final = ZCLAttributeDef(
            id=0x006F, type=t.bitmap8, access="r", mandatory=True
        )
        engineering_units: Final = ZCLAttributeDef(
            id=0x0075, type=t.enum16, access="r"
        )  # Does not seem to be in binary_output
        application_type: Final = ZCLAttributeDef(
            id=0x0100, type=t.uint32_t, access="r"
        )
        cluster_revision: Final = foundation.ZCL_CLUSTER_REVISION_ATTR
        reporting_status: Final = foundation.ZCL_REPORTING_STATUS_ATTR

class AzbMultistateOutput(CustomCluster):
    """Esp multistate output."""

    cluster_id: t.uint16_t = 0xFF13
    ep_attribute ="multistate_output"

    @property
    def _is_manuf_specific(self) -> bool:
        """Return True if cluster_id is within manufacturer specific range."""
        return False

    class AttributeDefs(BaseAttributeDefs): # Manually defining for now because zigpy doesn't follow the ZCL standard here
        state_text: Final = ZCLAttributeDef(
            id=0x000E, type=t.LVList[t.CharacterString, t.uint16_t], access="r*w"
        )
        description: Final = ZCLAttributeDef(
            id=0x001C, type=t.CharacterString, access="r*w"
        )
        number_of_states: Final = ZCLAttributeDef(
            id=0x004A, type=t.uint16_t, access="r*w", mandatory=True
        )
        out_of_service: Final = ZCLAttributeDef(
            id=0x0051, type=t.Bool, access="r*w", mandatory=True
        )
        present_value: Final = ZCLAttributeDef(
            id=0x0055, type=t.uint16_t, access="rwp", mandatory=True
        )
        # 0x0057: ZCLAttributeDef('priority_array', type=TODO.array),  # Array of 16 structures of (boolean,
        # unsigned 16bit int)
        reliability: Final = ZCLAttributeDef(id=0x0067, type=t.enum8, access="r*w")
        relinquish_default: Final = ZCLAttributeDef(
            id=0x0068, type=t.uint16_t, access="r*w"
        )
        status_flags: Final = ZCLAttributeDef(
            id=0x006F, type=t.bitmap8, access="r", mandatory=True
        )
        application_type: Final = ZCLAttributeDef(
            id=0x0100, type=t.uint32_t, access="r"
        )
        cluster_revision: Final = foundation.ZCL_CLUSTER_REVISION_ATTR
        reporting_status: Final = foundation.ZCL_REPORTING_STATUS_ATTR

(QuirkBuilder("Abominable Inc", "AC Controller")
.replaces(AzbMultistateOutput, endpoint_id=10)
.replaces(AzbBinaryOutput, endpoint_id=13)
.replaces(AzbBinaryOutput, endpoint_id=14)
.replaces(AzbBinaryOutput, endpoint_id=15)
.replaces(AzbMultistateOutput, endpoint_id=16)
.enum(attribute_name=AzbMultistateOutput.AttributeDefs.present_value.name,
      reporting_config=ReportingConfig(0, 30, 1),
      enum_class=Operation_Modes,
      endpoint_id=10,
      cluster_id=AzbMultistateOutput.cluster_id,
      entity_type=EntityType.STANDARD,
      translation_key="ac_mode",
      fallback_name="Mode")
.switch(attribute_name=AzbBinaryOutput.AttributeDefs.present_value.name,
        reporting_config=ReportingConfig(0, 30, 1),
        endpoint_id=13,
        cluster_id=AzbBinaryOutput.cluster_id,
        entity_type=EntityType.STANDARD,
        translation_key="use_external_thermometer",
        fallback_name="External Thermometer")
.switch(attribute_name=AzbBinaryOutput.AttributeDefs.present_value.name,
        reporting_config=ReportingConfig(0, 30, 1),
        endpoint_id=14,
        cluster_id=AzbBinaryOutput.cluster_id,
        entity_type=EntityType.STANDARD,
        translation_key="swing_louvre",
        fallback_name="Swing louvre")
.switch(attribute_name=AzbBinaryOutput.AttributeDefs.present_value.name,
        reporting_config=ReportingConfig(0, 30, 1),
        endpoint_id=15,
        cluster_id=AzbBinaryOutput.cluster_id,
        entity_type=EntityType.STANDARD,
        translation_key="sleep_mode",
        fallback_name="Sleep mode")
.enum(attribute_name=AzbMultistateOutput.AttributeDefs.present_value.name,
      reporting_config=ReportingConfig(0, 30, 1),
      enum_class=Fan_Modes,
      endpoint_id=16,
      cluster_id=AzbMultistateOutput.cluster_id,
      entity_type=EntityType.STANDARD,
      translation_key="fan_speed",
      fallback_name="Fan speed")
.add_to_registry())