//
// Created by charlie on 8/25/26.
//

#include "main.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <ranges>
#include <algorithm>

namespace EDA {
    namespace Core {
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

                        // std::cout << "Updating module:" << module->module_id << std::endl;
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
                    std::wcerr << "Over writing output port" <<std::endl;
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
                    return false;
                }

                module->inputs[input_index] = wire_id;

                if (!std::ranges::contains(wire->dependents, module_id)) {
                    wire->dependents.push_back(module_id);
                }

                return true;
            }

            bool bind_module_to_module(const ModuleId outputting_module_id, const int output_port, const ModuleId inputting_module_id, const int input_port) {
                const WireId newWireId = newWire();
                if (!bind_module_output(outputting_module_id,output_port,newWireId)) {
                    return false;
                }
                if (!bind_module_input(inputting_module_id,input_port,newWireId)) {
                    return false;
                }
                return true;
            }

            int getCurrentTime() const {
                return this->currentState.currentTime;
            }

            std::vector<Core::WireId> makeNWires(const int n) {
                auto wireIds = std::vector<WireId>(n);
                for (int i = 0; i < n; i++) {
                    wireIds[i] = newWire();
                }
                return wireIds;
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
        class Inverter : public Core::ModuleTypeDef {
        public:
            Inverter() {
                this->type_id = -1;
                this->input_cnt = 1;
                this->output_cnt = 1;
                this->name = "Inverter";
            }

            void update(Core::Module *module, Core::EDA_Environment *env) override {
                switch (env->get_wire(module->inputs[0])->state) {
                    case Core::WireState::LOW:
                        env->setWire(module->outputs[0], Core::WireState::HIGH, module->module_id);
                        break;
                    case Core::WireState::HIGH:
                        env->setWire(module->outputs[0], Core::WireState::LOW, module->module_id);
                        break;
                    default:
                        break;
                }
            }

            int inputUpdateDelay() override {
                return 1;
            }
        };

        class And : public Core::ModuleTypeDef {
        public:
            And() {
                this->type_id = -1;
                this->input_cnt = 2;
                this->output_cnt = 1;
                this->name = "Inverter";
            }

            void update(Core::Module *module, Core::EDA_Environment *env) override {
                Core::WireState a = env->get_wire(module->inputs[0])->state;
                Core::WireState b = env->get_wire(module->inputs[1])->state;
                env->setWire(module->outputs[0], a * b, module->module_id);
            }

            int inputUpdateDelay() override {
                return 1;
            }
        };

        class Log : public Core::ModuleTypeDef {
        public:
            Log(const char *prefix) {
                this->type_id = -1;
                this->input_cnt = 1;
                this->output_cnt = 0;
                this->name = "Logger";
                this->prefix = prefix;
            }

            void update(Core::Module *module, Core::EDA_Environment *env) override {
                std::cout << prefix << env->get_wire(module->inputs[0])->state << " @ " << env->getCurrentTime() <<
                        std::endl;
            }

            int inputUpdateDelay() override {
                return 1;
            }

            static Core::ModuleId attach_logger(Core::EDA_Environment* env, const Core::WireId wire_id, const char* prefix) {
                const Core::ModuleId id = env->generateModule(env->addModuleTypeDef(new Log(prefix)));
                env->bind_module_input(id, 0, wire_id);
                return id;
            }

        private:
            const char *prefix;
        };
    }

    namespace Dynamics {
        static Core::WireId ringOscillator(const int stages, Core::EDA_Environment *env) {
            const Core::ModuleTypeId inverter_id = env->addModuleTypeDef(new Primitive_Modules::Inverter());

            std::vector<Core::WireId> wires = env->makeNWires(stages);
            auto modules = std::vector<Core::ModuleId>(stages);

            for (int i = 0; i < stages; i++) {
                modules[i] = env->generateModule(inverter_id);
                env->bind_module_input(modules[i],0,wires[(i+stages-1) % stages]);
                env->bind_module_output(modules[i],0,wires[i]);
            }

            return wires[stages-1];
        }
    }
}

static void constexpr manualRingOscillator(EDA::Core::EDA_Environment* env) {
    using namespace EDA;

    Primitive_Modules::Inverter inverterDef;
    Primitive_Modules::And andDef;
    auto loggerDef = Primitive_Modules::Log("Output:");
    const Core::ModuleId not_type_id = env->addModuleTypeDef(&inverterDef);
    const Core::ModuleId and_type_id = env->addModuleTypeDef(&andDef);
    const Core::ModuleId out_log_type_id = env->addModuleTypeDef(&loggerDef);

    const Core::WireId test_wire1_id = env->newWire();
    const Core::WireId test_wire2_id = env->newWire();
    const Core::WireId test_wire3_id = env->newWire();

    const Core::ModuleId inverter1_id = env->generateModule(not_type_id);
    const Core::ModuleId inverter2_id = env->generateModule(not_type_id);
    const Core::ModuleId inverter3_id = env->generateModule(not_type_id);
    const Core::ModuleId logger_id = env->generateModule(out_log_type_id);

    env->bind_module_input(logger_id, 0, test_wire3_id);

    env->bind_module_input(inverter1_id, 0, test_wire3_id);
    env->bind_module_output(inverter1_id, 0, test_wire1_id);

    env->bind_module_input(inverter2_id, 0, test_wire1_id);
    env->bind_module_output(inverter2_id, 0, test_wire2_id);

    env->bind_module_input(inverter3_id, 0, test_wire2_id);
    env->bind_module_output(inverter3_id, 0, test_wire3_id);
}

int main() {
    using namespace EDA;
    Core::EDA_Environment env;

    auto loggerDef = Primitive_Modules::Log("Output:");
    Primitive_Modules::And andDef;
    const Core::ModuleTypeId type_id = env.addModuleTypeDef(&andDef);

    const Core::ModuleId and_module = env.generateModule(type_id);

    const Core::WireId osc1outId = Dynamics::ringOscillator(3, &env);
    const Core::WireId osc2outId = Dynamics::ringOscillator(5, &env);
    const Core::WireId andOut = env.newWire();

    env.bind_module_input(and_module, 0, osc1outId);
    env.bind_module_input(and_module, 1, osc2outId);
    env.bind_module_output(and_module, 0, andOut);

    Primitive_Modules::Log::attach_logger(&env, osc1outId, "Osc 1:");
    Primitive_Modules::Log::attach_logger(&env, osc2outId, "Osc 2:");
    Primitive_Modules::Log::attach_logger(&env, andOut, "And:");

    env.start();
    env.advance(50);

    return EXIT_SUCCESS;
}
