//
// Created by charlie on 9/2/26.
//
#include "Core.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <ranges>
#include <algorithm>
#include <format>

namespace Sim::Core {
    inline  bool isWireStateReal(const WireState state) {
        return state == WireState::HIGH || state == WireState::LOW;
    }

    inline bool isWireStateImg(const WireState state) {
        return !isWireStateReal(state);
    }

    std::ostream &operator<<(std::ostream& os, const WireState wire) {
        switch (wire) {
            case WireState::HIGH: os << "High";
                break;
            case WireState::LOW: os << "Low";
                break;
            case WireState::HI_Z: os << "Hi Z";
                break;
            case WireState::X: os << "X";
                break;
            default: os << "Unknown";
                break;
        }
        return os;
    }

    static WireState operator*(const WireState &other_wire, const WireState wire) {
        if (isWireStateImg(wire) || isWireStateImg(other_wire)) {
            return WireState::X;
        }

        if (other_wire == WireState::HIGH && wire == WireState::HIGH) {
            return WireState::HIGH;
        }
        return WireState::LOW;
    }

    ModuleTypeDef::ModuleTypeDef(const int id, const int input_cnt, const int output_cnt, const char *name) {
        this->type_id = id;
        this->input_cnt = input_cnt;
        this->output_cnt = output_cnt;
        this->name = name;
    }

    void ModuleTypeDef::update(Module *module, EDA_Environment *env) {
        std::cerr << "Module \"" << this->name << "\" not implemented" << std::endl;
    };

    int ModuleTypeDef::inputUpdateDelay() {
        return 1;
    }

    bool EDA_Environment::verify_modules() {
        for (const auto &device: this->module_map | std::views::keys) {
            if (!this->verify_module(device)) {
                return false;
            }
        }
        return true;
    }

    bool EDA_Environment::verify_module(const ModuleId module_id) {
        const auto moduleSearch = this->module_map.find(module_id);
        if (moduleSearch == this->module_map.end()) {
            std::cerr << "Module ID:" << module_id << " is not defined" << std::endl;
            return false;
        }
        const auto module = moduleSearch->second;

        const ModuleTypeDef *type_def = this->getModuleTypeDef(module.typeId);
        if (type_def == nullptr) {
            std::cerr << "Device type " << module.typeId << " not found!" << std::endl;
            return false;
        }

        if (std::ranges::contains(module.inputs, 0) != 0) {
            std::cerr << "Device id: " << module.typeId << std::endl <<
                    "Has a undefined input" << std::endl;
            return false;
        }

        if (module.inputs.size() != type_def->input_cnt) {
            std::cerr << "Device id: " << module.typeId << " has incorrect number of inputs" << std::endl <<
                    "A \"" << type_def->name << "\" requires " << type_def->input_cnt << " input(s)";
            return false;
        }

        if (module.outputs.size() != type_def->output_cnt || std::ranges::contains(module.outputs, 0) != 0) {
            std::cerr << "Device id: " << module.typeId << " has incorrect number of outputs" << std::endl <<
                    "A \"" << type_def->name << "\" requires " << type_def->output_cnt << " output(s)";
            return false;
        }

        return true;
    }

    ModuleTypeDef *EDA_Environment::getModuleTypeDef(const ModuleTypeId id) {
        if (const auto module = this->deviceDef_map.find(id); module != this->deviceDef_map.end()) {
            return module->second;
        }
        std::cerr << "Couldn't find module type" << std::endl << std::format("Module Type Id of {}", id) << std::endl<< std::endl;
        return nullptr;
    }

    int EDA_Environment::addModuleTypeDef(ModuleTypeDef *module) {
        if (module->type_id != -1 && this->deviceDef_map.contains(module->type_id)) {
            return -1;
        }

        if (module->type_id == -1) {
            // Ensures that next module id is not already used
            while (this->deviceDef_map.contains(this->next_moduletype_id_)) {
                this->next_moduletype_id_++;
            }

            module->type_id = this->next_moduletype_id_;
        }

        this->deviceDef_map[module->type_id] = module;
        return module->type_id;
    }

    WireId EDA_Environment::newWire() {
        const WireId wire_id = nextWire;
        Wire *wire = new Wire;
        this->wire_map[wire_id] = wire;
        nextWire++;
        return wire_id;
    }

    bool EDA_Environment::setWire(const WireId targetWire, const WireState newState, ModuleId setter) {
        auto it = wire_map.find(targetWire);
        if (it == wire_map.end()) {
            std::cerr << " Wire doesn't exist" << std::endl;
            return false;
        }

        if (Wire *wire = it->second; wire->setBy == setter || wire->state == WireState::HI_Z || wire->setBy == -
                                     1) {
            if (newState != wire->state) {
                // update dependents
                for (auto i: wire->dependents) {
                    queueModuleForUpdate(i);
                }
            }
            wire->state = newState;
            wire->setBy = setter;
        } else {
            std::cerr << "wire id:" << targetWire << " can not be set by " << setter;
            return false;
        }
        return true;
    }

    /**
     * Configure the simulation to start running.
     * @return If the simulation successfully passed verification checks.
     */
    bool EDA_Environment::start() {
        if (this->verify_modules()) {
            this->currentState.running = true;
            this->currentState.currentTime = 0;
            return true;
        }
        return false;
    }

    void EDA_Environment::queueModuleForUpdate(const ModuleId id) {
        Module *module = getModule(id);
        if (module == nullptr) {
            return;
        }
        queueModuleForUpdate(module, getModuleTypeDef(module->typeId));
    }

    void EDA_Environment::queueModuleForUpdate(Module *module, ModuleTypeDef *type) {
        module->next_update = currentState.currentTime + type->inputUpdateDelay();
        this->module_update_queue.push_back(module);
    }

    /**
     * Advances the environment the given number of time steps.
     * @param dt How many time steps to advance. Must be greater then 0.
     */
    void EDA_Environment::advance(const int dt) {
        for (int i = 0; i < dt; i++) {
            this->advance();
        }
    }

    /**
     * Advances the environment one time step.
     */
    void EDA_Environment::advance() {
        auto cached_update_queue = this->module_update_queue;
        this->module_update_queue = std::vector<Module *>();
        auto it = cached_update_queue.begin();
        while (it != cached_update_queue.end()) {
            Module *module = *it;
            if (module->next_update <= this->currentState.currentTime) {
                module->next_update = -1;
                const auto module_type = getModuleTypeDef(module->typeId);

                if (module_type == nullptr) {
                    std::cerr << "Module id:" << module->module_id << " type is not defined." << std::endl;
                    return;
                }

                module_type->update(module, this);
                cached_update_queue.erase(it);
            } else {
                ++it;
            }
        }
        this->module_update_queue.reserve(cached_update_queue.size() + this->module_update_queue.size());
        this->module_update_queue.insert(this->module_update_queue.end(), cached_update_queue.begin(),
                                         cached_update_queue.end());
        this->currentState.currentTime += 1;
    }

    /**
     * Creates a blank module instance of the given type.
     * @param typeId  Type id to generate a module of
     * @return id of the generated module
     */
    ModuleId EDA_Environment::newModule(const ModuleTypeId typeId) {
        if (this->currentState.running) {
            std::cerr << "Attempted to create module while running" << std::endl;
            return -1;
        }
        const auto it = deviceDef_map.find(typeId);
        if (it == deviceDef_map.end()) {
            throw std::invalid_argument(std::format("Could not find module type id:{}", typeId));
        }
        const ModuleTypeDef *type_def = it->second;
        const ModuleId module_id = this->next_module_id;
        auto new_module = Module(module_id, std::vector<WireId>(type_def->input_cnt),
                                 std::vector<WireId>(type_def->output_cnt), new std::string, 0, typeId);

        this->module_map[module_id] = new_module;
        this->module_map[module_id].next_update = 0;
        this->next_module_id++;
        this->module_update_queue.push_back(&this->module_map[module_id]);
        return module_id;
    }

    Module *EDA_Environment::getModule(const ModuleId module_id) {
        return &this->module_map[module_id];
    }

    Wire *EDA_Environment::get_wire(const WireId wire_id) {
        return wire_map[wire_id];
    }

    bool EDA_Environment::set_module_prefix(ModuleId id, const std::string *prefix) {
        this->module_prefixes.insert(*prefix);
        getModule(id)->prefix = &*this->module_prefixes.find(*prefix);

        return true;
    }

    bool EDA_Environment::bind_module_output(const ModuleId id, const int output_index, const WireId wire_id) {
        auto module = getModule(id);

        if (module == nullptr) {
            std::cerr << "Module doesn't exists" << std::endl;
            return false;
        }

        if (module->outputs.size() <= output_index) {
            std::cerr << "attempted to bind to invalid output index." << std::endl << "attempted " <<
                    output_index << " of " << module->outputs.size() << std::endl << std::endl;
            return false;
        }

        if (module->outputs[output_index] != 0) {
            std::wcerr << "Over writing output port" << std::endl;
        }

        module->outputs[output_index] = wire_id;
        return true;
    }

    bool EDA_Environment::bind_module_input(const ModuleId module_id, const int input_index, const WireId wire_id) {
        const auto module = getModule(module_id);
        auto wire = get_wire(wire_id);

        if (module == nullptr) {
            std::cerr << "Module doesn't exists" << std::endl;
            return false;
        }

        if (wire == nullptr) {
            std::cerr << "Wire doesn't exists" << std::endl;
            return false;
        }

        if (module->inputs.size() <= input_index) {
            std::cerr << "attempted to bind to invalid input index." << std::endl << "attempted " <<
                    input_index << " of " << module->outputs.size() << std::endl << std::endl;
            return false;
        }

        if (module->inputs[input_index] != 0) {
            std::wcerr << "Over writing output port";
            return false;
        }

        module->inputs[input_index] = wire_id;

        if (!std::ranges::contains(wire->dependents, module_id)) {
            wire->dependents.push_back(module_id);
        }

        return true;
    }

    bool EDA_Environment::bind_module_to_module(const ModuleId outputting_module_id, const int output_port,
                                                const ModuleId inputting_module_id, const int input_port) {
        const WireId newWireId = newWire();
        if (!bind_module_output(outputting_module_id, output_port, newWireId)) {
            return false;
        }
        if (!bind_module_input(inputting_module_id, input_port, newWireId)) {
            return false;
        }
        return true;
    }

    int EDA_Environment::getCurrentTime() const {
        return this->currentState.currentTime;
    }

    std::vector<WireId> EDA_Environment::newWires(const int n) {
        auto wireIds = std::vector<WireId>(n);
        for (int i = 0; i < n; i++) {
            wireIds[i] = newWire();
        }
        return wireIds;
    }
}
