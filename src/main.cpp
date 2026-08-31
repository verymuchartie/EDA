//
// Created by charlie on 8/25/26.
//

#include "main.h"
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <ranges>
#include <algorithm>

namespace EDA {
    namespace Env {
        class EDA_Environment;
        typedef int WireId;
        typedef int ModuleId;
        typedef int ModuleTypeId;

        struct SimState {
            int currentTime;
            bool running;
        };

        enum class WireState {
            HIGH, // Logic High
            LOW, // Logic Low
            HI_Z, // Not Connected
            X, // Unknown
        };

        static constexpr bool isWireStateReal(const WireState state) {
            return state == WireState::HIGH || state == WireState::LOW;
        }

        static constexpr bool isWireStateImg(const WireState state) {
            return !isWireStateReal(state);
        }

        // Overload the stream insertion operator
        static std::ostream &operator<<(std::ostream &os, const WireState wire) {
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

        struct Wire {
            WireId id{};
            WireState state{WireState::LOW};
            ModuleId setBy{-1}; // Which module last set this wire. -1 if HI_Z
            std::vector<ModuleId> dependents; // All the modules that use this wire as a input.
        };

        struct Module {
            ModuleId module_id{};
            std::vector<WireId> inputs;
            std::vector<WireId> outputs;
            int next_update{};
            ModuleTypeId typeId{};
        };

        class ModuleTypeDef {
        public:
            virtual ~ModuleTypeDef() = default;

            ModuleTypeDef() = default;

            ModuleTypeDef(const int id, const int input_cnt, const int output_cnt, const char *name) {
                this->type_id = id;
                this->input_cnt = input_cnt;
                this->output_cnt = output_cnt;
                this->name = name;
            }

            int type_id{};
            int input_cnt{};
            int output_cnt{};
            std::string name;

            virtual void update(Module *module, EDA_Environment *env) {
                std::cerr << "Module \"" << this->name << "\" not implemented" << std::endl;
            };

            /**
             * Returns how long the update should take.
             * @return How long the update should take in time steps.
             */
            virtual int inputUpdateDelay() {
                return 1;
            }
        };


        class EDA_Environment {
        public:
            EDA_Environment() = default;

            bool verify_modules() {
                for (const auto &device: this->module_map | std::views::keys) {
                    if (!this->verify_module(device)) {
                        return false;
                    }
                }
                return true;
            }

            bool verify_module(const ModuleId module_id) {
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

                if (module.inputs.size() != type_def->input_cnt || std::ranges::contains(module.inputs, 0) != 0) {
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

            ModuleTypeDef *getModuleTypeDef(const ModuleTypeId id) {
                if (const auto module = this->deviceDef_map.find(id); module != this->deviceDef_map.end()) {
                    return module->second;
                }
                std::cout << "Couldn't find type" << std::endl;
                return nullptr;
            }

            int addModuleTypeDef(ModuleTypeDef *module) {
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

            WireId newWire() {
                const WireId wire_id = nextWire;
                Wire *wire = new Wire;
                this->wire_map[wire_id] = wire;
                nextWire++;
                return wire_id;
            }

            bool setWire(const WireId targetWire, const WireState newState, ModuleId setter) {
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
            bool start() {
                if (this->verify_modules()) {
                    this->currentState.running = true;
                    this->currentState.currentTime = 0;
                    return true;
                }
                return false;
            }

            void queueModuleForUpdate(const ModuleId id) {
                Module *module = getModule(id);
                if (module == nullptr) {
                    return;
                }
                queueModuleForUpdate(module, getModuleTypeDef(module->typeId));
            }

            void queueModuleForUpdate(Module *module, ModuleTypeDef *type) {
                module->next_update = currentState.currentTime + type->inputUpdateDelay();
                this->module_update_queue.push_back(module);
            }

            /**
             * Advances the environment the given number of time steps.
             * @param dt How many time steps to advance. Must be greater then 0.
             */
            void advance(const int dt) {
                for (int i = 0; i < dt; i++) {
                    this->advance();
                }
            }

            /**
             * Advances the environment one time step.
             */
            void advance() {
                std::cout << std::endl << "Starting step" << this->currentState.currentTime << std::endl;
                auto cached_update_queue = this->module_update_queue;
                this->module_update_queue = std::vector<Module*>();
                auto it = cached_update_queue.begin();
                while (it != cached_update_queue.end()) {
                    Module *module = *it;
                    if (module->next_update <= this->currentState.currentTime) {
                        module->next_update = -1;
                        const auto module_type = getModuleTypeDef(module->typeId);

                        if (module_type == nullptr) {
                            std::cerr << "Module id:" << module->module_id << " type is not defined."<<std::endl;
                            return;
                        }
                        module_type->update(module, this);
                        cached_update_queue.erase(it);
                    }else {
                        ++it;
                    }
                }
                this->module_update_queue.reserve(cached_update_queue.size() + this->module_update_queue.size());
                this->module_update_queue.insert(this->module_update_queue.end(),cached_update_queue.begin(), cached_update_queue.end());
                this->currentState.currentTime += 1;
            }

            /**
             * Creates a blank module instance of the given type.
             * @param typeId  Type id to generate a module of
             * @return id of the generated module
             */
            ModuleId generateModule(const ModuleTypeId typeId) {
                if (this->currentState.running) {
                    std::cerr << "Attempted to generate module while running" << std::endl;
                    return -1;
                }
                const auto it = deviceDef_map.find(typeId);
                if (it == deviceDef_map.end()) {
                    std::cerr << "Device Type Id:" << typeId << " Not defined";
                }
                const ModuleTypeDef *type_def = it->second;
                const ModuleId module_id = this->next_module_id;
                auto new_module = Module(module_id, std::vector<WireId>(type_def->input_cnt),
                                         std::vector<WireId>(type_def->output_cnt), 0, typeId);

                this->module_map[module_id] = new_module;
                this->module_map[module_id].next_update = 0;
                this->next_module_id++;
                this->module_update_queue.push_back(&this->module_map[module_id]);
                return module_id;
            }

            Module *getModule(const ModuleId module_id) {
                return &this->module_map[module_id];
            }

            Wire *get_wire(const WireId wire_id) {
                return wire_map[wire_id];
            }

            bool bind_module_output(const ModuleId id, const int output_index, const WireId wire_id) {
                auto module = getModule(id);
                auto wire = get_wire(wire_id);

                if (module == nullptr) {
                    std::cerr << "Module doesn't exists" << std::endl;
                    return false;
                }

                if (wire == nullptr) {
                    std::cerr << "Wire doesn't exists" << std::endl;
                    return false;
                }

                if (module->outputs.size() <= output_index) {
                    std::cerr << "attempted to bind to invalid output index." << std::endl << "attempted " <<
                            output_index << " of " << module->outputs.size() << std::endl << std::endl;
                    return false;
                }

                if (module->outputs[output_index] != 0) {
                    std::wcerr << "Over writing output port";
                }

                module->outputs[output_index] = wire_id;
                return true;
            }

            bool bind_module_input(const ModuleId module_id, const int input_index, const WireId wire_id) {
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
                }

                module->inputs[input_index] = wire_id;

                if (!std::ranges::contains(wire->dependents, module_id)) {
                    wire->dependents.push_back(module_id);
                }

                return true;
            }

        private:
            SimState currentState{.currentTime = 0, .running = false};

            int nextWire = 1;
            std::unordered_map<int, Wire *> wire_map;

            int next_module_id = 1000;
            std::unordered_map<int, Module> module_map;

            int next_moduletype_id_ = 2000;
            std::unordered_map<int, ModuleTypeDef *> deviceDef_map;
            std::vector<Module *> module_update_queue;
        };
    }


    namespace Primitive_Modules {
        class Inverter : public Env::ModuleTypeDef {
        public:
            Inverter() {
                this->type_id = -1;
                this->input_cnt = 1;
                this->output_cnt = 1;
                this->name = "Inverter";
            }

            void update(Env::Module *module, Env::EDA_Environment *env) override {
                switch (env->get_wire(module->inputs[0])->state) {
                    case Env::WireState::LOW:
                        env->setWire(module->outputs[0], Env::WireState::HIGH, module->module_id);
                        break;
                    case Env::WireState::HIGH:
                        env->setWire(module->outputs[0], Env::WireState::LOW, module->module_id);
                        break;
                    default:
                        break;
                }
            }

            int inputUpdateDelay() override {
                return 1;
            }
        };

        class And : public Env::ModuleTypeDef {
        public:
            And() {
                this->type_id = -1;
                this->input_cnt = 2;
                this->output_cnt = 1;
                this->name = "Inverter";
            }

            void update(Env::Module *module, Env::EDA_Environment *env) override {
                Env::WireState a = env->get_wire(module->inputs[0])->state;
                Env::WireState b = env->get_wire(module->inputs[1])->state;
                env->setWire(module->outputs[0], a * b, module->module_id);
            }

            int inputUpdateDelay() override {
                return 1;
            }
        };

        class Log : public Env::ModuleTypeDef {
        public:
            Log(const char *prefix) {
                this->type_id = -1;
                this->input_cnt = 1;
                this->output_cnt = 0;
                this->name = "Logger";
                this->prefix = prefix;
            }

            void update(Env::Module *module, Env::EDA_Environment *env) override {
                std::cout << prefix << env->get_wire(module->inputs[0])->state;
            }

            int inputUpdateDelay() override {
                return 1;
            }

        private:
            const char *prefix;
        };
    }
}

int main() {
    using namespace EDA;
    Env::EDA_Environment env;
    Primitive_Modules::Inverter invertedDef;
    Primitive_Modules::And andDef;
    Primitive_Modules::Log logger = Primitive_Modules::Log("Output:");

    const Env::ModuleId not_type_id = env.addModuleTypeDef(&invertedDef);
    const Env::ModuleId and_type_id = env.addModuleTypeDef(&andDef);
    const Env::ModuleId out_log_type_id = env.addModuleTypeDef(&logger);

    const Env::WireId test_wire_id = env.newWire();

    const Env::ModuleId inverter_id = env.generateModule(not_type_id);
    const Env::ModuleId logger_id = env.generateModule(out_log_type_id);

    env.bind_module_input(logger_id, 0, test_wire_id);

    env.bind_module_input(inverter_id, 0, test_wire_id);
    env.bind_module_output(inverter_id, 0, test_wire_id);

    env.start();
    env.advance(5);

    return EXIT_SUCCESS;
}
