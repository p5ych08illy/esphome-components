import esphome.codegen as cg
from esphome.components import climate_ir

CODEOWNERS = ["@p5ych08illy"]
AUTO_LOAD = ["climate_ir"]

fisher_ns = cg.esphome_ns.namespace("fisher_ir")
FisherClimate = fisher_ns.class_("FisherClimate", climate_ir.ClimateIR)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FisherClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)
