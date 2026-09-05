//
// Created by charlie on 8/25/26.
//

#include "sim/Core.h"
#include "sim/Primitive_Modules.h"
#include <cmath>

#include "sim/Composite_Module.h"
#include "sim/Dynamics.h"

static void manualRingOscillator(Sim::Core::EDA_Environment *env) {
    using namespace Sim;

    Core::ModuleTypeDef* inverterDef = Primitive_Modules::inverterTypeDef();
    Core::ModuleTypeDef* andDef = Primitive_Modules::andTypeDef();
    auto* loggerDef = new Primitive_Modules::Log("Output:");

    const Core::ModuleId not_type_id = env->addModuleTypeDef(inverterDef);
    const Core::ModuleId and_type_id = env->addModuleTypeDef(andDef);
    const Core::ModuleId out_log_type_id = env->addModuleTypeDef(loggerDef);

    const Core::WireId test_wire1_id = env->newWire();
    const Core::WireId test_wire2_id = env->newWire();
    const Core::WireId test_wire3_id = env->newWire();

    const Core::ModuleId inverter1_id = env->newModule(not_type_id);
    const Core::ModuleId inverter2_id = env->newModule(not_type_id);
    const Core::ModuleId inverter3_id = env->newModule(not_type_id);
    const Core::ModuleId logger_id = env->newModule(out_log_type_id);

    env->bind_module_input(logger_id, 0, test_wire3_id);

    env->bind_module_input(inverter1_id, 0, test_wire3_id);
    env->bind_module_output(inverter1_id, 0, test_wire1_id);

    env->bind_module_input(inverter2_id, 0, test_wire1_id);
    env->bind_module_output(inverter2_id, 0, test_wire2_id);

    env->bind_module_input(inverter3_id, 0, test_wire2_id);
    env->bind_module_output(inverter3_id, 0, test_wire3_id);
}

static void dualingRingOsc(Sim::Core::EDA_Environment* env) {
    using namespace Sim;
    auto loggerDef = Primitive_Modules::Log("Output:");
    auto andDef = Primitive_Modules::andTypeDef();
    const Core::ModuleTypeId and_type_id = env->addModuleTypeDef(andDef);

    const Core::ModuleId and_module = env->newModule(and_type_id);

    const Core::WireId osc1outId = Dynamics::ringOscillator(3, env);
    const Core::WireId osc2outId = Dynamics::ringOscillator(5, env);
    const Core::WireId andOut = env->newWire();

    env->bind_module_input(and_module, 0, osc1outId);
    env->bind_module_input(and_module, 1, osc2outId);
    env->bind_module_output(and_module, 0, andOut);

    Primitive_Modules::Log::attach_logger(env, osc1outId, "Osc 1:");
    Primitive_Modules::Log::attach_logger(env, osc2outId, "Osc 2:");
    Primitive_Modules::Log::attach_logger(env, andOut, "And:");
}


static void compositeModuleTest(Sim::Core::EDA_Environment *env) {
    using namespace Sim;

    Core::ModuleTypeDef* inverterDef = Primitive_Modules::inverterTypeDef();
    Core::ModuleTypeDef* andDef = Primitive_Modules::andTypeDef();
    auto* loggerDef = new Primitive_Modules::Log("Output:");

    auto composite_module = new Core::Composite::Composite_Module();

    const Core::Composite::Submodule_Id not_type_id = env->addModuleTypeDef(inverterDef);
    const Core::Composite::Submodule_Id out_log_type_id = env->addModuleTypeDef(loggerDef);

    const Core::Composite::Composite_WireId test_wire1_id = composite_module->new_wire();
    const Core::Composite::Composite_WireId test_wire2_id = composite_module->new_wire();
    const Core::Composite::Composite_WireId test_wire3_id = composite_module->new_wire();

    const Core::Composite::Submodule_Id inverter1_id = composite_module->new_module(not_type_id);
    const Core::Composite::Submodule_Id inverter2_id = composite_module->new_module(not_type_id);
    const Core::Composite::Submodule_Id inverter3_id = composite_module->new_module(not_type_id);
    const Core::Composite::Submodule_Id logger_id = composite_module->new_module(out_log_type_id);

    composite_module->bind_composite_module_input(logger_id, 0, test_wire3_id);

    composite_module->bind_composite_module_input(inverter1_id, 0, test_wire3_id);
    composite_module->bind_composite_module_output(inverter1_id, 0, test_wire1_id);

    composite_module->bind_composite_module_input(inverter2_id, 0, test_wire1_id);
    composite_module->bind_composite_module_output(inverter2_id, 0, test_wire2_id);

    composite_module->bind_composite_module_input(inverter3_id, 0, test_wire2_id);
    composite_module->bind_composite_module_output(inverter3_id, 0, test_wire3_id);

    composite_module->generate_instance(env);
    composite_module->generate_instance(env);
}

int main() {
    using namespace Sim;
    Core::EDA_Environment env;

    compositeModuleTest(&env);

    env.start();
    env.advance(50);

    return EXIT_SUCCESS;
}
