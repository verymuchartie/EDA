//
// Created by charlie on 9/4/26.
//

#include "Composite_Module_Def.h"

#include <ranges>
#include <string>

namespace Sim::Core::Composite {
    Virtual_WireId Composite_Module_Def::new_wire() {
        Virtual_Wire &composite_wire = virtual_wires.emplace_back();
        composite_wire.virtual_wire_id = this->nextWireId;
        ++this->nextWireId;
        return composite_wire.virtual_wire_id;
    }

    std::vector<Virtual_WireId> Composite_Module_Def::new_wires(const int n) {
        auto composite_wire_ids = std::vector<Virtual_WireId>(n);
        for (int i = 0; i < n; i++) {
            composite_wire_ids[i] = new_wire();
        }
        return composite_wire_ids;
    }

    Submodule_Id Composite_Module_Def::new_module(ModuleTypeId id) {
        auto& submodule = this->submodules.emplace_back();
        submodule.composite_module_id = nextSubmoduleId;
        submodule.module_type_id = id;

        ++nextSubmoduleId;
        return submodule.composite_module_id;
    }

    bool Composite_Module_Def::bind_submodule_output(Submodule_Id id, int output_index, Virtual_WireId wire_id) {
        const auto composite_wire_it = std::ranges::find_if(virtual_wires.begin(), virtual_wires.end(),
                                                            [wire_id](const Virtual_Wire &wire) {
                                                                return wire_id == wire.virtual_wire_id;
                                                            });
        if (composite_wire_it == virtual_wires.end()) {
            std::cerr << "Composite Wire Not found!" << std::endl;
            return false;
        }

        composite_wire_it->outputs.emplace_back(id, output_index);
        return true;
    }

    bool Composite_Module_Def::bind_submodule_input(Submodule_Id id, int input_index, Virtual_WireId wire_id) {
        const auto composite_wire_it = std::ranges::find_if(virtual_wires.begin(), virtual_wires.end(),
                                                            [wire_id](const Virtual_Wire &wire) {
                                                                return wire_id == wire.virtual_wire_id;
                                                            });
        if (composite_wire_it == virtual_wires.end()) {
            std::cerr << "Composite Wire Not found!" << std::endl;
            return false;
        }

        composite_wire_it->inputs.emplace_back(id, input_index);
        return true;
    }

    bool Composite_Module_Def::generate_instance(EDA_Environment* env) {
        return bind_modules_to_wires(env, generate_instance_modules(env), generate_instance_wires(env));
    }

    Composite_Module_Def::Composite_Module_Wires* Composite_Module_Def::generate_instance_wires(EDA_Environment *env) {
        const auto wires = new Composite_Module_Wires;

        for (auto &wire: this->virtual_wires) {
            if (-1 == wire.virtual_wire_id) { // Wire is uninitialized
                break;
            }

            wires->concrete_wire_ids[wire.virtual_wire_id] = env->newWire();
        }

        return wires;
    }

    Composite_Module_Def::Composite_Module_Submodules* Composite_Module_Def::generate_instance_modules(EDA_Environment *env) {
        const auto concrete_submodules = new Composite_Module_Submodules;
        for (auto &i : this->submodules) {
            ModuleId newModuleId = env->newModule(i.module_type_id);
            env->set_module_prefix(newModuleId, &this->name);
            concrete_submodules->concrete_module_ids[i.composite_module_id] = newModuleId;
        }
        return concrete_submodules;
    }

    bool Composite_Module_Def::bind_modules_to_wires(EDA_Environment *env, Composite_Module_Submodules *submodule,
        Composite_Module_Wires *wires) {
        for (auto &wire : this->virtual_wires) {
            WireId concrete_wireId = wires->concrete_wire_ids[wire.virtual_wire_id];

            if (-1 == wire.virtual_wire_id) {
                break;
            }

            for (const auto &input : wire.inputs) {
                if (!env->bind_module_input(submodule->concrete_module_ids[input.first], input.second, concrete_wireId)) {
                    std::cerr << "failed to bind module input." << std::endl;
                    return false;
                }
            }

            for (const auto &output : wire.outputs) {
                if (!env->bind_module_output(submodule->concrete_module_ids[output.first], output.second, concrete_wireId)) {
                    std::cerr << "failed to bind module output." << std::endl;
                    return false;
                }
            }
        }
        return true;
    }


    const std::string *Composite_Module_Def::getName() const {
        return &this->name;
    }

    Composite_Module_Def::Submodule * Composite_Module_Def::get_submodule(Submodule_Id id) {
        const auto submodule = std::ranges::find_if(submodules.begin(), submodules.end(),
                                                            [id](const Submodule &s_module) {
                                                                return id == s_module.composite_module_id;
                                                            });
        if (submodule == submodules.end()) {
            throw std::invalid_argument("Could not find sub module.");
        }

        return submodule.base();
    }
}
