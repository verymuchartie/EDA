//
// Created by charlie on 9/4/26.
//

#include "Composite_Module.h"

#include <ranges>
#include <string>

namespace Sim::Core::Composite {
    Composite_WireId Composite_Module::new_wire() {
        Composite_Wire &composite_wire = composite_wires.emplace_back();
        composite_wire.com_wire_id = this->nextWireId;
        ++this->nextWireId;
        return composite_wire.com_wire_id;
    }

    std::vector<Composite_WireId> Composite_Module::new_wires(const int n) {
        auto composite_wire_ids = std::vector<Composite_WireId>(n);
        for (int i = 0; i < n; i++) {
            composite_wire_ids[i] = new_wire();
        }
        return composite_wire_ids;
    }

    Submodule_Id Composite_Module::new_module(ModuleTypeId id) {
        auto& submodule = this->submodules.emplace_back();
        submodule.composite_module_id = nextSubmoduleId;
        submodule.module_type_id = id;

        ++nextSubmoduleId;
        return submodule.composite_module_id;
    }

    bool Composite_Module::bind_composite_module_output(Submodule_Id id, int output_index, Composite_WireId wire_id) {
        const auto composite_wire_it = std::ranges::find_if(composite_wires.begin(), composite_wires.end(),
                                                            [wire_id](const Composite_Wire &wire) {
                                                                return wire_id == wire.com_wire_id;
                                                            });
        if (composite_wire_it == composite_wires.end()) {
            std::cerr << "Composite Wire Not found!" << std::endl;
            return false;
        }

        composite_wire_it->outputs.emplace_back(id, output_index);
        return true;
    }

    bool Composite_Module::bind_composite_module_input(Submodule_Id id, int input_index, Composite_WireId wire_id) {
        const auto composite_wire_it = std::ranges::find_if(composite_wires.begin(), composite_wires.end(),
                                                            [wire_id](const Composite_Wire &wire) {
                                                                return wire_id == wire.com_wire_id;
                                                            });
        if (composite_wire_it == composite_wires.end()) {
            std::cerr << "Composite Wire Not found!" << std::endl;
            return false;
        }

        composite_wire_it->inputs.emplace_back(id, input_index);
        return true;
    }

    bool Composite_Module::generate_instance(EDA_Environment* env) {
        for (auto &wire : this->composite_wires) {
            if (-1 == wire.com_wire_id) { // Wire is uninitialized
                break;
            }
            const auto new_wire_id = env->newWire();
            wire.concrete_wire_id = new_wire_id;
        }

        for (auto& submodule : this->submodules) {
            if (-1 == submodule.module_type_id) {
                break;
            }
            const auto new_module_id = env->newModule(submodule.module_type_id);
            if (-1 == new_module_id) {
                std::cerr << "Environment missing underlying module type." << std::endl;
                return false;
            }
            submodule.concrete_module_id = new_module_id;
        }

        for (auto &wire : this->composite_wires) {
            if (-1 == wire.com_wire_id) {
                break;
            }
            for (const auto &input : wire.inputs) {
                if (!env->bind_module_input(get_submodule(input.first)->concrete_module_id, input.second, wire.concrete_wire_id)) {
                    std::cerr << "failed to bind module input." << std::endl;
                    return false;
                }
            }

            for (const auto &output : wire.outputs) {
                if (!env->bind_module_output(get_submodule(output.first)->concrete_module_id, output.second, wire.concrete_wire_id)) {
                    std::cerr << "failed to bind module output." << std::endl;
                    return false;
                }
            }
        }

        return true;
    }

    const std::string *Composite_Module::getName() const {
        return &this->name;
    }

    Composite_Module::Submodule * Composite_Module::get_submodule(Submodule_Id id) {
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
