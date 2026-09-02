//
// Created by charlie on 9/2/26.
//

#include "Dynamics.h"

#include "Primitive_Modules.h"

namespace Sim::Dynamics {
    Core::WireId ringOscillator(const int stages, Core::EDA_Environment *env) {
        const Core::ModuleTypeId inverter_id = env->addModuleTypeDef(Primitive_Modules::inverterTypeDef());

        std::vector<Core::WireId> wires = env->makeNWires(stages);
        auto modules = std::vector<Core::ModuleId>(stages);

        for (int i = 0; i < stages; i++) {
            modules[i] = env->generateModule(inverter_id);
            env->bind_module_input(modules[i], 0, wires[(i + stages - 1) % stages]);
            env->bind_module_output(modules[i], 0, wires[i]);
        }

        return wires[stages - 1];
    }
}
